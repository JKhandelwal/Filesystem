/*
  MyFS. One directory, one file, 1000 bytes of storage. What more do you need?

  This Fuse file system is based largely on the HelloWorld example by Miklos
  Szeredi <miklos@szeredi.hu> (http://fuse.sourceforge.net/helloworld.html).
  Additional inspiration was taken from Joseph J. Pfeiffer's "Writing a FUSE
  Filesystem: a Tutorial" (http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/).
*/

#define FUSE_USE_VERSION 26

#include <errno.h>
#include <fcntl.h>
#include <fuse.h>

#include "myfs.h"

// The one and only fcb that this implmentation will have. We'll keep it in
// memory. A better
// implementation would, at the very least, cache it's root directroy in memory.
uuid_t root_uuid;
myfcb the_root_fcb;
unqlite_int64 root_object_size_value = sizeof(myfcb);

// This is the pointer to the database we will use to store all our files
unqlite *pDb;
uuid_t zero_uuid;

int getFCBFromPath(const char *path, myfcb *returnFCB) {
  char charPth[strlen(path) + 1];
  strcpy(charPth, path);

  char *token = strtok(charPth, "/");
  unqlite_int64 nBytes;
  int result;
  myfcb value = the_root_fcb;

  while (token != NULL) {
    if (!S_ISDIR(value.mode)) {
      if (strtok(NULL, "/") != NULL) {
        return -ENOENT;
      }
      break;
    } else {
      int count = (value.size) / sizeof(dirent);
      // if the root directory does not contain any files;
      if (count == 0) {
        return -ENOENT;
      }
      dirent dirValue[count];
      nBytes = sizeof(dirent) * count;
      result = unqlite_kv_fetch(pDb, value.file_data_id, KEY_SIZE, dirValue,
                                &nBytes);
      if (result != UNQLITE_OK)
        return -ENOENT;

      bool found = false;
      int index;
      for (int i = 0; i < count; i++) {
        if (strcmp(dirValue[i].name, token) == 0) {
          found = true;
          index = i;
          token = strtok(NULL, "/");
          break;
        }
      }
      if (!found)
        return -ENOENT;
      nBytes = sizeof(myfcb);
      result = unqlite_kv_fetch(pDb, dirValue[index].referencedFCB, KEY_SIZE,
                                &value, &nBytes);
    }
  }

  *returnFCB = value;
  return 0;
}

// The functions which follow are handler functions for various things a
// filesystem needs to do:
// reading, getting attributes, truncating, etc. They will be called by FUSE
// whenever it needs
// your filesystem to do something, so this is where functionality goes.

// Get file and directory attributes (meta-data).
// Read 'man 2 stat' and 'man 2 chmod'.
static int myfs_getattr(const char *path, struct stat *stbuf) {

  write_log("myfs_getattr(path=\"%s\", statbuf=0x%08x)\n", path, stbuf);

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    // conceptually the root
    stbuf->st_mode = the_root_fcb.mode;
    stbuf->st_nlink = 2;
    stbuf->st_uid = the_root_fcb.uid;
    stbuf->st_gid = the_root_fcb.gid;
    stbuf->st_mtime = the_root_fcb.mtime;
    stbuf->st_ctime = the_root_fcb.ctime;
    stbuf->st_size = the_root_fcb.size;
  } else {

    myfcb curr;
    memset(&curr, 0, sizeof(myfcb));
    int res = getFCBFromPath(path, &curr);
    if (res < 0) {
      write_log("Stat did not find %s ", path);
      return -ENOENT;
    }
    write_log("Stat found: %s ", path);
    // if (curr.name)
    stbuf->st_mode = curr.mode;
    stbuf->st_nlink = 1;
    stbuf->st_mtime = curr.mtime;
    stbuf->st_ctime = curr.ctime;
    stbuf->st_size = curr.size;
    stbuf->st_uid = curr.uid;
    stbuf->st_gid = curr.gid;
    // }else{
    // write_log("myfs_getattr - ENOENT");
    // return 0;
    // }
  }
  return 0;
}

int getParentUUID(uuid_t *uuid, const char *path) {
  char copy[strlen(path) + 1];
  strcpy(copy, path);

  char *parentDir = dirname(copy);
  char parentCpy[strlen(parentDir) + 1];
  strcpy(parentCpy, parentDir);
  char *parentName = basename(parentCpy);
  write_log("Parent Name is %s \n",parentName);
  char *parentDirOfParentDir = dirname(parentDir);
  write_log("Parents Parent is %s \n",parentDirOfParentDir);
  if (strcmp(parentName,"/") == 0) {
    // TOFIX
    write_log("get Parent UUID copies the root Object key %s \n", ROOT_OBJECT_KEY);
    uuid_copy(*uuid, ROOT_OBJECT_KEY);
    write_log("New Root Object is %s\n",*uuid);
    return 0;
  }
  myfcb parParFCB;
  int result = 0;
  int retVal = getFCBFromPath(parentDirOfParentDir, &parParFCB);
  if (retVal < 0)
    return -1;
  int count = (parParFCB.size) / sizeof(dirent);
  // if the root directory does not contain any files;
  if (count == 0) {
    return -ENOENT;
  }
  unqlite_int64 nBytes = parParFCB.size;
  dirent dirValue[count];
  nBytes = sizeof(dirent) * count;
  result = unqlite_kv_fetch(pDb, parParFCB.file_data_id, KEY_SIZE, dirValue,
                            &nBytes);
  if (result != UNQLITE_OK)
    return -ENOENT;

  for (int i = 0; i < count; i++) {
    if (strcmp(dirValue[i].name, parentName) == 0) {
      uuid_copy(*uuid, dirValue[i].referencedFCB);
      return 0;
    }
  }
  return -1;
}

// Create a directory.
// Read 'man 2 mkdir'.
int myfs_mkdir(const char *path, mode_t mode) {
  write_log("myfs_mkdir: %s\n", path);
  bool isRoot = false;
  char copy[strlen(path) + 1];
  strcpy(copy, path);
  mode |= S_IFDIR;
  int rc;
  uuid_t parUUID;
  unqlite_int64 nBytes = sizeof(myfcb);
  char *name = basename(copy);
  strcpy(copy, path);
  rc = getParentUUID(&parUUID, path);
  write_log("mkdir GETS THE PARENT UUID the parent uuid is %s\n",parUUID);
  // if (rc ==5)
  if (rc < 0) {
    return -ENOENT;
  }
  bool updateCacheRoot = false;
  if (uuid_compare(parUUID,ROOT_OBJECT_KEY) ==0){
    updateCacheRoot =true;
  }
  myfcb parentFCB;
  nBytes = sizeof(myfcb);
  write_log("uuid is %s length of the key is %d nBytes is %d\n",parUUID,KEY_SIZE,nBytes );
  rc = unqlite_kv_fetch(pDb, parUUID, KEY_SIZE, &parentFCB, &nBytes);
  if (rc != UNQLITE_OK) {
    write_log("Fetch Seems to be failing\n");
    return -1;
  }
  write_log("mkdir Fetches the Parent UUID \n");

  // Size of 0 represents that the directory does not contain any values
  myfcb newFCB;
  uuid_copy(newFCB.file_data_id, zero_uuid);
  newFCB.size = 0;
  newFCB.ctime = time(NULL);
  newFCB.mtime = time(NULL);
  newFCB.mode = mode;
  newFCB.uid = getuid();
  newFCB.gid = getgid();
  uuid_t uid;
  uuid_generate(uid);
  rc = unqlite_kv_store(pDb, uid, KEY_SIZE, &newFCB, sizeof(myfcb));
  if (rc != UNQLITE_OK) {
    return -1;
  }
  write_log("mkdir stores the new FCB\n");

  dirent newDirent;
  strcpy(newDirent.name, name);
  uuid_copy(newDirent.referencedFCB, uid);

  uuid_t direntUUID;
  if (parentFCB.size == 0) {
    uuid_generate(direntUUID);
    uuid_copy(parentFCB.file_data_id, direntUUID);
    if (updateCacheRoot == true){
      uuid_copy(the_root_fcb.file_data_id,direntUUID);
    }
  } else {
    uuid_copy(direntUUID, parentFCB.file_data_id);
  }

  rc = unqlite_kv_append(pDb, direntUUID, KEY_SIZE, &newDirent, sizeof(dirent));
  if (rc != UNQLITE_OK) {
    return -ENOENT;
  }

  parentFCB.size += sizeof(dirent);
  if (isRoot) {
    the_root_fcb.size += sizeof(dirent);
  }
  if (updateCacheRoot == true){
    the_root_fcb.size += sizeof(dirent);
  }
  int retVal =
      unqlite_kv_store(pDb, parUUID, KEY_SIZE, &parentFCB, sizeof(myfcb));
  if (rc != UNQLITE_OK) {
    return -ENOENT;
  }
  return 0;
}

// Read a directory.
// Read 'man 2 readdir'.
static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi) {

  (void)offset; // This prevents compiler warnings
  (void)fi;

  write_log("write_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, "
            "offset=%lld, fi=0x%08x)\n",
            path, buf, filler, offset, fi);
  // info on filler()
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
myfcb directory;
    write_log("readdir is a directory\n");
  int result = getFCBFromPath(path, &directory);
  if (result < 0) {
    return -ENOENT;
  }
  if (!S_ISDIR(directory.mode))
    return -1;
  int count = directory.size / sizeof(dirent);
  if (count == 0)
    return 0;
  unqlite_int64 nBytes = sizeof(dirent) * count;

    dirent dirents[count];
  result = unqlite_kv_fetch(pDb, directory.file_data_id, KEY_SIZE, &dirents,
                            &nBytes);
  if (result != UNQLITE_OK)
    return -ENOENT;

//TODO Change to stat struct rather than fuse struct
  nBytes = sizeof(myfcb);
  for (int i = 0; i < count; i++) {
    filler(buf, dirents[i].name, NULL, 0);
  }

  return 0;
}

// Delete a directory.
// Read 'man 2 rmdir'.
int myfs_rmdir(const char *path) {
  write_log("myfs_rmdir: %s\n", path);
  char copy [strlen(path) +1];
  strcpy(copy,path);
  char* rmDir = basename(copy);
  uuid_t parUUID;
  int result = getParentUUID(&parUUID,path);
  myfcb parFCB;
  unqlite_int64 nBytes= sizeof(myfcb);
  result = unqlite_kv_fetch(pDb,parUUID,KEY_SIZE,&parFCB,&nBytes);
  if (result != UNQLITE_OK || nBytes != sizeof(myfcb)){
    write_log("The unqlite fetch of the parent fcb failed\n");
    return result;
  }
  bool isRoot = false;
  if (uuid_compare(ROOT_OBJECT_KEY,parUUID) ==0){
    isRoot = true;
  }

  int count = parFCB.size/sizeof(dirent);

  if (count ==0){
    return -ENOENT;
  } else {
    nBytes = sizeof(dirent) * count;

    dirent dirents[count];
    result = unqlite_kv_fetch(pDb, parFCB.file_data_id, KEY_SIZE, &dirents,
                             &nBytes);
    if (result != UNQLITE_OK || nBytes != (sizeof(dirent) * count))return -ENOENT;
    if (count ==1){
      myfcb delFCB;
      nBytes = sizeof(myfcb);
      result = unqlite_kv_fetch(pDb,dirents[0].referencedFCB,KEY_SIZE,&delFCB,&nBytes);
      if (result != UNQLITE_OK || nBytes != sizeof(myfcb))return -ENOENT;
      if (delFCB.size != 0) return -ENOTEMPTY;
      result = unqlite_kv_delete(pDb,parFCB.file_data_id,KEY_SIZE);
      if (result != UNQLITE_OK){
        return -1;
      }
      uuid_copy(parFCB.file_data_id,zero_uuid);
      if (isRoot == true){
        uuid_copy(the_root_fcb.file_data_id,zero_uuid);
      }
    } else {
      int index  =0;
      for (int i =0;i < count;i++){
        if (strcmp(rmDir,dirents[i].name) ==0) {
          index = i;
          myfcb delFCB;
          nBytes = sizeof(myfcb);
          result = unqlite_kv_fetch(pDb,dirents[i].referencedFCB,KEY_SIZE,&delFCB,&nBytes);
          if (result != UNQLITE_OK || nBytes != sizeof(myfcb))return -ENOENT;
          if (delFCB.size != 0) return -ENOTEMPTY;

          break;
        }
      }

      result = unqlite_kv_delete(pDb,dirents[index].referencedFCB,KEY_SIZE);
      if (result != UNQLITE_OK){
        return -1;
      }
      if (index != (count-1)){
        strcpy(dirents[index].name,dirents[count-1].name);
        uuid_copy(dirents[index].referencedFCB,dirents[count-1].referencedFCB);
      }
      result = unqlite_kv_store(pDb,parFCB.file_data_id,KEY_SIZE,&dirents,sizeof(dirent) *(count -1));
      if (result != UNQLITE_OK) return -1;
    }
    if (isRoot == true){
      the_root_fcb.size -= sizeof(dirent);
    }
    parFCB.size -= sizeof(dirent);
    nBytes = sizeof(myfcb);
    result = unqlite_kv_store(pDb,parUUID,KEY_SIZE,&parFCB,sizeof(myfcb));
    if (result != UNQLITE_OK || nBytes != sizeof(myfcb)) return -1;
  }
  //TODO
  //Remove the Record from the parent
  //Delete the record

  return 0;
}


// Read a file.
// Read 'man 2 read'.
static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
  size_t len;
  (void)fi;

  write_log(
      "myfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
      path, buf, size, offset, fi);
      char copy[strlen(path) + 1];
      strcpy(copy, path);
      int rc;
      uuid_t parUUID;
      unqlite_int64 nBytes = sizeof(myfcb);
      char *name = basename(copy);
      strcpy(copy, path);
      rc = getParentUUID(&parUUID, path);
      write_log("read GETS THE PARENT UUID the parent uuid is %s\n",parUUID);
      if (rc < 0) {
        return -ENOENT;
      }
      myfcb parentFCB;
      nBytes = sizeof(myfcb);
      write_log("uuid is %s length of the key is %d nBytes is %d\n",parUUID,KEY_SIZE,nBytes );
      rc = unqlite_kv_fetch(pDb, parUUID, KEY_SIZE, &parentFCB, &nBytes);
      if (rc != UNQLITE_OK) {
        write_log("Fetch Seems to be failing\n");
        return -1;
      }
      int count = parentFCB.size / sizeof(dirent);
      if (count <1) return -1;
      dirent dirents[count];
      nBytes = sizeof(dirent) *count;
      rc = unqlite_kv_fetch(pDb, parentFCB.file_data_id, KEY_SIZE, &dirents,
                               &nBytes);
      if (rc != UNQLITE_OK || nBytes != (sizeof(dirent) * count))return -ENOENT;

      uuid_t writeUUID;
      for (int i = 0; i < count;i++){
        if (strcmp(name,dirents[i].name) ==0){
          uuid_copy(writeUUID,dirents[i].referencedFCB);
          break;
        }
      }
      myfcb referencedFCB;
      nBytes = sizeof(myfcb);
      rc = unqlite_kv_fetch(pDb, writeUUID,KEY_SIZE,&referencedFCB,&nBytes);
      if (rc != UNQLITE_OK || nBytes != sizeof(myfcb)) return -1;
      write_log("seems to be able to get past the setup\n");
      char * buffer = malloc(referencedFCB.size +1);
      write_log("after the mallocing\n");
      nBytes = referencedFCB.size;
      write_log("the requested read size %d,offset is %d,size is %d\n",offset +size,offset,size);
      int actualsize =size;
      if (size +offset > referencedFCB.size){
        actualsize = referencedFCB.size - offset;
      }
      rc = unqlite_kv_fetch(pDb, referencedFCB.file_data_id,KEY_SIZE,buffer,&nBytes);
      if (rc != UNQLITE_OK || nBytes != referencedFCB.size){
        write_log("fetch of the buffer is failing\n");
        free(buffer);
        return -1;
      }
      write_log("fetches the buffer, offset is %d, actualsize is %d\n",offset,actualsize);
      write_log("the start is %p, %p\n\n",buffer, buffer+offset);
      memcpy(buf,buffer+offset,actualsize);
      write_log("copies into the buffer\n");

      free(buffer);
      return actualsize;
             // if (offset > referencedFCB.size)

  // return size;
}

// This file system only supports one file. Create should fail if a file has
// been created. Path must be '/<something>'.
// Read 'man 2 creat'.
static int myfs_create(const char *path, mode_t mode,
                       struct fuse_file_info *fi) {
  write_log("myfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode,
            fi);
            write_log("myfs_create: %s\n", path);
            bool isRoot = false;
            char copy[strlen(path) + 1];
            strcpy(copy, path);
            int rc;
            uuid_t parUUID;
            unqlite_int64 nBytes = sizeof(myfcb);
            char *name = basename(copy);
            strcpy(copy, path);
            rc = getParentUUID(&parUUID, path);
            write_log("mkdir GETS THE PARENT UUID the parent uuid is %s\n",parUUID);
            // if (rc ==5)
            if (rc < 0) {
              return -ENOENT;
            }
            bool updateCacheRoot = false;
            if (uuid_compare(parUUID,ROOT_OBJECT_KEY) ==0){
              updateCacheRoot =true;
            }
            myfcb parentFCB;
            nBytes = sizeof(myfcb);
            write_log("uuid is %s length of the key is %d nBytes is %d\n",parUUID,KEY_SIZE,nBytes );
            rc = unqlite_kv_fetch(pDb, parUUID, KEY_SIZE, &parentFCB, &nBytes);
            if (rc != UNQLITE_OK) {
              write_log("Fetch Seems to be failing\n");
              return -1;
            }
            write_log("mkdir Fetches the Parent UUID \n");

            // Size of 0 represents that the directory does not contain any values
            myfcb newFCB;
            uuid_t temp;
            uuid_generate(temp);
            uuid_copy(newFCB.file_data_id, temp);
            newFCB.size = 0;
            newFCB.ctime = time(NULL);
            newFCB.mtime = time(NULL);
            newFCB.mode = mode;
            newFCB.uid = getuid();
            newFCB.gid = getgid();
            uuid_t uid;
            uuid_generate(uid);
            rc = unqlite_kv_store(pDb, uid, KEY_SIZE, &newFCB, sizeof(myfcb));
            if (rc != UNQLITE_OK) {
              return -1;
            }
            write_log("mkdir stores the new FCB\n");

            dirent newDirent;
            strcpy(newDirent.name, name);
            uuid_copy(newDirent.referencedFCB, uid);

            uuid_t direntUUID;
            if (parentFCB.size == 0) {
              uuid_generate(direntUUID);
              uuid_copy(parentFCB.file_data_id, direntUUID);
              if (updateCacheRoot == true){
                uuid_copy(the_root_fcb.file_data_id,direntUUID);
              }
            } else {
              uuid_copy(direntUUID, parentFCB.file_data_id);
            }

            rc = unqlite_kv_append(pDb, direntUUID, KEY_SIZE, &newDirent, sizeof(dirent));
            if (rc != UNQLITE_OK) {
              return -ENOENT;
            }

            parentFCB.size += sizeof(dirent);
            if (isRoot) {
              the_root_fcb.size += sizeof(dirent);
            }
            if (updateCacheRoot == true){
              the_root_fcb.size += sizeof(dirent);
            }
            int retVal =
                unqlite_kv_store(pDb, parUUID, KEY_SIZE, &parentFCB, sizeof(myfcb));
            if (rc != UNQLITE_OK) {
              return -ENOENT;
            }

  return 0;
}

// Set update the times (actime, modtime) for a file. This FS only supports
// modtime.
// Read 'man 2 utime'.
static int myfs_utime(const char *path, struct utimbuf *ubuf) {
  write_log("myfs_utime(path=\"%s\", ubuf=0x%08x)\n", path, ubuf);
  // // TODO
  // // if(strcmp(path, the_root_fcb.path) != 0){
  // // 	write_log("myfs_utime - ENOENT");
  // // 	return -ENOENT;
  // // }
  //
  //
  // // Write the fcb to the store.
  // int rc = unqlite_kv_store(pDb, ROOT_OBJECT_KEY, KEY_SIZE,
  //                           &the_root_fcb, sizeof(myfcb));
  // if (rc != UNQLITE_OK) {
  //   write_log("myfs_write - EIO");
  //   return -EIO;
  // }
  //
  // return 0;
  // write_log("mode is %s\n",mode);
  char name[strlen(path)+1];
  strcpy(name,path);
  char*  base = basename(name);
  uuid_t parUUID;
  int res = getParentUUID(&parUUID,path);
  if (res < 0) return -1;
  myfcb parFCB;
  unqlite_int64 nBytes = sizeof(myfcb);
  res = unqlite_kv_fetch(pDb,parUUID,KEY_SIZE,&parFCB,&nBytes);

  if (res != UNQLITE_OK || nBytes != sizeof(myfcb)){
    return -1;
  }

  int count = parFCB.size/sizeof(dirent);

  if (count < 1) return -1;

  dirent dirs[count];
  nBytes = sizeof(dirent) *count;
  res = unqlite_kv_fetch(pDb,parFCB.file_data_id,KEY_SIZE,&dirs,&nBytes);

  if (res != UNQLITE_OK || nBytes != sizeof(dirent) *count){
    return -1;
  }

  myfcb FCB;
  nBytes = sizeof(myfcb);
  for (int i =0;i<count;i++){
    if (strcmp(dirs[i].name,base) ==0){
      res = unqlite_kv_fetch(pDb,dirs[i].referencedFCB,KEY_SIZE,&FCB,&nBytes);
      if (res != UNQLITE_OK || nBytes != sizeof(myfcb)){
        return -1;
      }
      // FCB.mode = mode;
        FCB.mtime = ubuf->modtime;
      res = unqlite_kv_store(pDb,dirs[i].referencedFCB,KEY_SIZE,&FCB,sizeof(myfcb));
      if (res != UNQLITE_OK){
        return -1;
      }
      return 0;
    }
  }

  return -1;
}

// Write to a file.
// Read 'man 2 write'
static int myfs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi) {
  write_log(
      "myfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
      path, buf, size, offset, fi);
  char copy[strlen(path) + 1];
  strcpy(copy, path);
  int rc;
  uuid_t parUUID;
  unqlite_int64 nBytes = sizeof(myfcb);
  char *name = basename(copy);
  strcpy(copy, path);
  rc = getParentUUID(&parUUID, path);
  write_log("write GETS THE PARENT UUID the parent uuid is %s\n",parUUID);
  if (rc < 0) {
    return -ENOENT;
  }
  myfcb parentFCB;
  nBytes = sizeof(myfcb);
  write_log("uuid is %s length of the key is %d nBytes is %d\n",parUUID,KEY_SIZE,nBytes );
  rc = unqlite_kv_fetch(pDb, parUUID, KEY_SIZE, &parentFCB, &nBytes);
  if (rc != UNQLITE_OK) {
    write_log("Fetch Seems to be failing\n");
    return -1;
  }
  int count = parentFCB.size / sizeof(dirent);
  if (count <1) return -1;
  dirent dirents[count];
  nBytes = sizeof(dirent) *count;
  rc = unqlite_kv_fetch(pDb, parentFCB.file_data_id, KEY_SIZE, &dirents,
                           &nBytes);
  if (rc != UNQLITE_OK || nBytes != (sizeof(dirent) * count))return -ENOENT;

  uuid_t writeUUID;
  for (int i = 0; i < count;i++){
    if (strcmp(name,dirents[i].name) ==0){
      uuid_copy(writeUUID,dirents[i].referencedFCB);
      break;
    }
  }
  myfcb referencedFCB;
  nBytes = sizeof(myfcb);
  rc = unqlite_kv_fetch(pDb, writeUUID,KEY_SIZE,&referencedFCB,&nBytes);
  if (rc != UNQLITE_OK || nBytes != sizeof(myfcb)) return -1;
  size_t oldSize = referencedFCB.size;
  if (offset > oldSize) return -1;
  write_log("seems to be getting after the setup\n\n");

  char* oldBuffer;
  char* newBuffer = malloc(offset + size);
  write_log("Offset %d + size %d is %d\n",offset,size,offset+size);
  if (oldSize  != 0){
    write_log("the size is %d\n",oldSize);
  oldBuffer = malloc(oldSize);
  nBytes = oldSize;
  rc = unqlite_kv_fetch(pDb,referencedFCB.file_data_id,KEY_SIZE,oldBuffer,&nBytes);
  if (rc != UNQLITE_OK || nBytes != oldSize) {
    free(oldBuffer);
    free(newBuffer);
    return -1;
  }
}
  write_log("it gets the old value\n");
  /*4 cases exist:
  1.) offset is 0, and the new write size is less than or equal to the old size -> just write it
  2.) offset is 0, and the new write size if greater than file size, increase the buffer and write it
  3.) offset isn't 0 and the new write size + offset is less than  or equal to the file size, in which case write it
  4.) offset isn't 0 and the new write size + offset is greater than the file size extend buffer and copy.
  */
  if ((offset + size) <= oldSize){
    write_log("It goes into less!\n");
    memcpy(offset+oldBuffer,buf,size);
    rc = unqlite_kv_store(pDb,referencedFCB.file_data_id,KEY_SIZE,oldBuffer,oldSize);
    if (rc != UNQLITE_OK) {
      free(oldBuffer);
      free(newBuffer);
      return -1;
    }
  } else {
    write_log("It goes into more\n");
    if (oldSize != 0){
      write_log("before the memcpy, oldsize is %d, offset is %d, oldPointer is %p new pointer is %p\n",oldSize,offset,oldBuffer,newBuffer);
      memcpy(newBuffer,oldBuffer,offset);
      write_log("After the memcpy\n");
    }
    write_log("Write is failing after original memcpy\n\n");
    memcpy(newBuffer+offset,buf, size);
    write_log("Does the memcpy\n");
    rc = unqlite_kv_store(pDb,referencedFCB.file_data_id,KEY_SIZE,newBuffer,offset+size);
    if (rc != UNQLITE_OK) {
      write_log("It borked out writing to the new buffer\n");
      free(oldBuffer);
      free(newBuffer);
      return -1;
    }
    referencedFCB.size = offset +size;
    write_log("Sets the new size\n");
  }
  write_log("after the if statement\n");
  rc = unqlite_kv_store(pDb,writeUUID,KEY_SIZE,&referencedFCB,sizeof(myfcb));
  if (rc != UNQLITE_OK) {
    write_log("error writing the fcb back\n");
    free(oldBuffer);
    free(newBuffer);
    return -1;
  }
  if (oldSize != 0)free(oldBuffer);
  free(newBuffer);
  write_log("\n\n\nit wrote to a file the size is %d\n\n\n",referencedFCB.size);
  return size;
}

// Set the size of a file.
// Read 'man 2 truncate'.
int myfs_truncate(const char *path, off_t newsize) {
  write_log("myfs_truncate(path=\"%s\", newsize=%lld)\n", path, newsize);
  char copy[strlen(path) + 1];
  strcpy(copy, path);
  int rc;
  uuid_t parUUID;
  unqlite_int64 nBytes = sizeof(myfcb);
  char *name = basename(copy);
  strcpy(copy, path);
  rc = getParentUUID(&parUUID, path);
  write_log("write GETS THE PARENT UUID the parent uuid is %s\n",parUUID);
  if (rc < 0) {
    return -ENOENT;
  }
  myfcb parentFCB;
  nBytes = sizeof(myfcb);
  write_log("uuid is %s length of the key is %d nBytes is %d\n",parUUID,KEY_SIZE,nBytes );
  rc = unqlite_kv_fetch(pDb, parUUID, KEY_SIZE, &parentFCB, &nBytes);
  if (rc != UNQLITE_OK) {
    write_log("Fetch Seems to be failing\n");
    return -1;
  }
  int count = parentFCB.size / sizeof(dirent);
  if (count <1) return -1;
  dirent dirents[count];
  nBytes = sizeof(dirent) *count;
  rc = unqlite_kv_fetch(pDb, parentFCB.file_data_id, KEY_SIZE, &dirents,
                           &nBytes);
  if (rc != UNQLITE_OK || nBytes != (sizeof(dirent) * count))return -ENOENT;

  uuid_t writeUUID;
  for (int i = 0; i < count;i++){
    if (strcmp(name,dirents[i].name) ==0){
      uuid_copy(writeUUID,dirents[i].referencedFCB);
      break;
    }
  }
  myfcb referencedFCB;
  nBytes = sizeof(myfcb);
  rc = unqlite_kv_fetch(pDb, writeUUID,KEY_SIZE,&referencedFCB,&nBytes);
  if (rc != UNQLITE_OK || nBytes != sizeof(myfcb));
  if (S_ISDIR(referencedFCB.mode)) return -1;
  if (newsize == referencedFCB.size) return 0;
  char* oldBuffer;
  if (referencedFCB.size != 0){
    oldBuffer = malloc(referencedFCB.size);
    nBytes = referencedFCB.size;
    rc = unqlite_kv_fetch(pDb,referencedFCB.file_data_id,KEY_SIZE,oldBuffer,&nBytes);
    if (rc != UNQLITE_OK || nBytes != referencedFCB.size){
        free(oldBuffer);
        return -1;
    }
  }
  write_log("The old buffer has/hasn't been created\n");
  char* buffer = malloc(newsize);
  write_log("The new Buffer has been created\n");

  if (newsize > referencedFCB.size){
    if (referencedFCB.size != 0)memcpy((void*) buffer, oldBuffer,referencedFCB.size);
    memset((buffer + referencedFCB.size),'\0',newsize - referencedFCB.size);
    write_log("Sets the memory \n");
    rc = unqlite_kv_store(pDb,referencedFCB.file_data_id,KEY_SIZE,buffer,newsize);
    if (rc != UNQLITE_OK){
      free(buffer);
      if (referencedFCB.size != 0)free(oldBuffer);
      return -1;
    }
    write_log("The file hasbeen written to memory\n");

  } else {
    rc = unqlite_kv_store(pDb,referencedFCB.file_data_id,KEY_SIZE,oldBuffer,newsize);
    if (rc != UNQLITE_OK){
      free(buffer);
      if (referencedFCB.size != 0)free(oldBuffer);
      return -1;
    }
    write_log("The old buffer has/hasn't been created\n");

  }
  int oldSize = referencedFCB.size;
  referencedFCB.size = newsize;
  rc = unqlite_kv_store(pDb,writeUUID,KEY_SIZE,&referencedFCB,sizeof(myfcb));

  if (rc != UNQLITE_OK){
    free(buffer);
    if (referencedFCB.size != 0)free(oldBuffer);
    return -1;
  }
  write_log("The fcb is written to memory\n");
  if (oldSize != 0)free(oldBuffer);
  free(buffer);
  return 0;

}

// Set permissions.
// Read 'man 2 chmod'.
int myfs_chmod(const char *path, mode_t mode) {
  write_log("myfs_chmod(fpath=\"%s\", mode=0%03o)\n", path, mode);
  // write_log("mode is %s\n",mode);
  char name[strlen(path)+1];
  strcpy(name,path);
  char*  base = basename(name);
  uuid_t parUUID;
  int res = getParentUUID(&parUUID,path);
  if (res <0) return -1;
  myfcb parFCB;
  unqlite_int64 nBytes = sizeof(myfcb);
  res = unqlite_kv_fetch(pDb,parUUID,KEY_SIZE,&parFCB,&nBytes);

  if (res != UNQLITE_OK || nBytes != sizeof(myfcb)){
    return -1;
  }

  int count = parFCB.size/sizeof(dirent);

  if (count < 1) return -1;

  dirent dirs[count];
  nBytes = sizeof(dirent) *count;
  res = unqlite_kv_fetch(pDb,parFCB.file_data_id,KEY_SIZE,&dirs,&nBytes);

  if (res != UNQLITE_OK || nBytes != sizeof(dirent) *count){
    return -1;
  }

  myfcb FCB;
  nBytes = sizeof(myfcb);
  for (int i =0;i<count;i++){
    if (strcmp(dirs[i].name,base) ==0){
      res = unqlite_kv_fetch(pDb,dirs[i].referencedFCB,KEY_SIZE,&FCB,&nBytes);
      if (res != UNQLITE_OK || nBytes != sizeof(myfcb)){
        return -1;
      }
      FCB.mode = mode;

      res = unqlite_kv_store(pDb,dirs[i].referencedFCB,KEY_SIZE,&FCB,sizeof(myfcb));
      if (res != UNQLITE_OK){
        return -1;
      }
      return 0;
    }
  }

  return -1;
}

// Set ownership.
// Read 'man 2 chown'.
int myfs_chown(const char *path, uid_t uid, gid_t gid) {
  write_log("myfs_chown(path=\"%s\", uid=%d, gid=%d)\n", path, uid, gid);
  char name[strlen(path)+1];
  strcpy(name,path);
  char*  base = basename(name);
  uuid_t parUUID;
  int res = getParentUUID(&parUUID,path);
  if (res <0) return -1;
  myfcb parFCB;
  unqlite_int64 nBytes = sizeof(myfcb);
  res = unqlite_kv_fetch(pDb,parUUID,KEY_SIZE,&parFCB,&nBytes);

  if (res != UNQLITE_OK || nBytes != sizeof(myfcb)){
    return -1;
  }

  int count = parFCB.size/sizeof(dirent);

  if (count < 1) return -1;

  dirent dirs[count];
  nBytes = sizeof(dirent) *count;
  res = unqlite_kv_fetch(pDb,parFCB.file_data_id,KEY_SIZE,&dirs,&nBytes);

  if (res != UNQLITE_OK || nBytes != sizeof(dirent) *count){
    return -1;
  }

  myfcb FCB;
  nBytes = sizeof(myfcb);
  for (int i =0;i<count;i++){
    if (strcmp(dirs[i].name,base) ==0){
      res = unqlite_kv_fetch(pDb,dirs[i].referencedFCB,KEY_SIZE,&FCB,&nBytes);
      if (res != UNQLITE_OK || nBytes != sizeof(myfcb)){
        return -1;
      }
      FCB.uid = uid;
      FCB.gid = gid;

      res = unqlite_kv_store(pDb,dirs[i].referencedFCB,KEY_SIZE,&FCB,sizeof(myfcb));
      if (res != UNQLITE_OK){
        return -1;
      }
      return 0;
    }
  }

  return -1;
}

// Delete a file.
// Read 'man 2 unlink'.
int myfs_unlink(const char *path) {
  write_log("myfs_unlink: %s\n", path);
  write_log("myfs_rmdir: %s\n", path);
  char copy [strlen(path) +1];
  strcpy(copy,path);
  char* rmDir = basename(copy);
  uuid_t parUUID;
  int result = getParentUUID(&parUUID,path);
  myfcb parFCB;
  unqlite_int64 nBytes= sizeof(myfcb);
  result = unqlite_kv_fetch(pDb,parUUID,KEY_SIZE,&parFCB,&nBytes);
  if (result != UNQLITE_OK || nBytes != sizeof(myfcb)){
    write_log("The unqlite fetch of the parent fcb failed\n");
    return result;
  }
  bool isRoot = false;
  if (uuid_compare(ROOT_OBJECT_KEY,parUUID) ==0){
    isRoot = true;
  }

  int count = parFCB.size/sizeof(dirent);

  if (count ==0){
    return -ENOENT;
  } else {
    write_log("Gets inside the count not being 0\n");
    nBytes = sizeof(dirent) * count;

    dirent dirents[count];
    result = unqlite_kv_fetch(pDb, parFCB.file_data_id, KEY_SIZE, &dirents,
                             &nBytes);
    if (result != UNQLITE_OK || nBytes != (sizeof(dirent) * count))return -ENOENT;
    if (count ==1){
      myfcb delFCB;
      nBytes = sizeof(myfcb);
      result = unqlite_kv_fetch(pDb,dirents[0].referencedFCB,KEY_SIZE,&delFCB,&nBytes);
      if (result != UNQLITE_OK || nBytes != sizeof(myfcb))return -ENOENT;
      // if (delFCB.size != 0) return -ENOTEMPTY;
      if (delFCB.size != 0){
      result = unqlite_kv_delete(pDb,parFCB.file_data_id,KEY_SIZE);
      if (result != UNQLITE_OK){
        return -1;
      }
    }
      uuid_copy(parFCB.file_data_id,zero_uuid);
      if (isRoot == true){
        uuid_copy(the_root_fcb.file_data_id,zero_uuid);
      }
    } else {
      int index  =0;
      for (int i =0;i < count;i++){
        if (strcmp(rmDir,dirents[i].name) ==0) {
          index = i;
          myfcb delFCB;
          nBytes = sizeof(myfcb);
          result = unqlite_kv_fetch(pDb,dirents[i].referencedFCB,KEY_SIZE,&delFCB,&nBytes);
          if (result != UNQLITE_OK || nBytes != sizeof(myfcb))return -ENOENT;
          if (delFCB.size > 0){
          result = unqlite_kv_delete(pDb,delFCB.file_data_id,KEY_SIZE);
          if (result != UNQLITE_OK){
            return -1;
          }
        }
          break;
        }
      }
      result = unqlite_kv_delete(pDb,dirents[index].referencedFCB,KEY_SIZE);
      if (result != UNQLITE_OK){
        return -1;
      }
      if (index != (count-1)){
        strcpy(dirents[index].name,dirents[count-1].name);
        uuid_copy(dirents[index].referencedFCB,dirents[count-1].referencedFCB);
      }
      result = unqlite_kv_store(pDb,parFCB.file_data_id,KEY_SIZE,&dirents,sizeof(dirent) *(count -1));
      if (result != UNQLITE_OK) return -1;
    }
    if (isRoot == true){
      the_root_fcb.size -= sizeof(dirent);
    }
    parFCB.size -= sizeof(dirent);
    nBytes = sizeof(myfcb);
    result = unqlite_kv_store(pDb,parUUID,KEY_SIZE,&parFCB,sizeof(myfcb));
    if (result != UNQLITE_OK || nBytes != sizeof(myfcb)) return -1;
  }
  //TODO
  //Remove the Record from the parent
  //Delete the record
  return 0;
}


// OPTIONAL - included as an example
// Flush any cached data.
int myfs_flush(const char *path, struct fuse_file_info *fi) {
  int retstat = 0;

  write_log("myfs_flush(path=\"%s\", fi=0x%08x)\n", path, fi);

  return retstat;
}

// OPTIONAL - included as an example
// Release the file. There will be one call to release for each call to open.
int myfs_release(const char *path, struct fuse_file_info *fi) {
  int retstat = 0;

  write_log("myfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

  return retstat;
}

// OPTIONAL - included as an example
// Open a file. Open should check if the operation is permitted for the given
// flags (fi->flags).
// Read 'man 2 open'.
static int myfs_open(const char *path, struct fuse_file_info *fi) {
  // TODO
  // if (strcmp(path, the_root_fcb.path) != 0)
  // 	return -ENOENT;

  write_log("myfs_open(path\"%s\", fi=0x%08x)\n", path, fi);

  // return -EACCES if the access is not permitted.

  return 0;
}

// This struct contains pointers to all the functions defined above
// It is used to pass the function pointers to fuse
// fuse will then execute the methods as required
static struct fuse_operations myfs_oper = {
    .getattr = myfs_getattr,
    .readdir = myfs_readdir,
    .mkdir = myfs_mkdir,
    .rmdir = myfs_rmdir,
    .open = myfs_open,
    .read = myfs_read,
    .create = myfs_create,
    .utime = myfs_utime,
    .write = myfs_write,
    .truncate = myfs_truncate,
    .flush = myfs_flush,
    .release = myfs_release,
    .chmod = myfs_chmod,
    .unlink = myfs_unlink,
};

// Initialise the in-memory data structures from the store. If the root object
// (from the store) is empty then create a root fcb (directory)
// and write it to the store. Note that this code is executed outide of fuse. If
// there is a failure then we have failed toi initlaise the
// file system so exit with an error code.
void init_fs() {
  int rc;
  printf("init_fs\n");
  // Open the database.
  rc = unqlite_open(&pDb, DATABASE_NAME, UNQLITE_OPEN_CREATE);
  if (rc != UNQLITE_OK)
    error_handler(rc);

  unqlite_int64 nBytes = sizeof(myfcb); // Data length

  // Try to fetch the root element
  // The last parameter is a pointer to a variable which will hold the number of
  // bytes actually read
  rc = unqlite_kv_fetch(pDb, ROOT_OBJECT_KEY, KEY_SIZE,
                        &the_root_fcb, &nBytes);

  // if it doesn't exist, we need to create one and put it into the database.
  // This will be the root
  // directory of our filesystem i.e. "/"
  if (rc == UNQLITE_NOTFOUND) {

    printf("init_store: root object was not found\n");

    // clear everything in the_root_fcb
    memset(&the_root_fcb, 0, sizeof(myfcb));

    // Sensible initialisation for the root FCB
    // See 'man 2 stat' and 'man 2 chmod'.
    the_root_fcb.mode |=
        S_IFDIR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    the_root_fcb.mtime = time(0);
    the_root_fcb.uid = getuid();
    the_root_fcb.gid = getgid();

    // Write the root FCB
    printf("init_fs: writing root fcb\n");
    rc = unqlite_kv_store(pDb, ROOT_OBJECT_KEY, KEY_SIZE,
                          &the_root_fcb, sizeof(myfcb));

    if (rc != UNQLITE_OK)
      error_handler(rc);
  } else {
    if (rc == UNQLITE_OK) {
      printf("init_store: root object was found\n");
    }
    if (nBytes != sizeof(myfcb)) {
      printf("Data object has unexpected size. Doing nothing.\n");
      exit(-1);
    }
  }
}

void shutdown_fs() { unqlite_close(pDb); }

int main(int argc, char *argv[]) {
  int fuserc;
  struct myfs_state *myfs_internal_state;

  // Setup the log file and store the FILE* in the private data object for the
  // file system.
  myfs_internal_state = malloc(sizeof(struct myfs_state));
  myfs_internal_state->logfile = init_log_file();

  // Initialise the file system. This is being done outside of fuse for ease of
  // debugging.
  init_fs();

  // Now pass our function pointers over to FUSE, so they can be called whenever
  // someone
  // tries to interact with our filesystem. The internal state contains a file
  // handle
  // for the logging mechanism
  fuserc = fuse_main(argc, argv, &myfs_oper, myfs_internal_state);

  // Shutdown the file system.
  shutdown_fs();

  return fuserc;
}
