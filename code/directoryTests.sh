#if [ ${F} != "/cs/scratch/jk218/mnt/" ]
#then
# echo "Error Path is"
# echo ${F}
# echo "It should be: /cs/scratch/jk218/mnt/ "
#else
# s=${F}
#
s="/cs/scratch/jk218/mnt/"
testPassed="true"
# Make folders a,b,c,d
var=$s"a"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"b"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"c"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"d"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi


#Put directories in a
var=$s"a/b"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"a/c"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"a/d"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi


#Put directories in b
var=$s"b/a"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"b/c"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"b/d"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi


#Put directories in c
var=$s"c/a"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"c/b"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"c/d"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi



#Put directories in d
var=$s"d/a"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"d/b"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi

var=$s"d/c"
mkdir $var
if [ -z $var ]
then
echo "Failed Making: $var"
testPassed=false
fi


#Remove the first directory in a
#Should have 4 directories, 2 non deleted and . and ..
# The directories c and d should still exist
var=$s"a/b"
rmdir $var
num=$(ls -a $s"a" | wc -l)
if [ $num -ne 4 ]
then
  echo "Not the right number of directories in the system after removing $var"
  testPassed=false
fi
if [ -e $var ]
then
  echo "Directory: $var still exists"
  testPassed=false
fi

var=$s"a/c"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi
var=$s"a/d"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi



#Remove the middle directory in b
#Should have 4 directories, 2 non deleted and . and ..
#The directories a, and d should still exist
var=$s"b/c"
rmdir $var
num=$(ls -a $s"b" | wc -l)
if [ $num -ne 4 ]
then
  echo "Not the right number of directories in the system after removing $var"
  testPassed=false
fi
if [ -e $var ]
then
  echo "Directory: $var still exists"
  testPassed=false
fi

var=$s"b/a"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi
var=$s"b/d"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi



#Remove the last/third directory in c
#Should have 4 directories, 2 non deleted and . and ..
#Directories a and b should still exist in c
var=$s"c/d"
rmdir $var
num=$(ls -a $s"c" | wc -l)
if [ $num -ne 4 ]
then
  echo "Not the right number of directories in the system after removing $var"
  testPassed=false
fi
if [ -e $var ]
then
  echo "Directory: $var still exists"
  testPassed=false
fi

var=$s"c/a"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi
var=$s"c/b"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi


#Remove all directories in d,
#Should have 2 files, . and ..
var=$s"d/a"
rmdir $var
if [ -e $var ]
then
  echo "Directory: $var still exists"
  testPassed=false
fi

var=$s"d/b"
rmdir $var
if [ -e $var ]
then
  echo "Directory: $var still exists"
  testPassed=false
fi

var=$s"d/c"
rmdir $var
if [ -e $var ]
then
  echo "Directory: $var still exists"
  testPassed=false
fi

num=$(ls -a $s"d" | wc -l)
if [ $num -ne 2 ]
then
  echo "Not the right number of directories in the system after removing $var"
  testPassed=false
fi


#now delete the directory d  which does not contain any files
var=$s"d"
rmdir $var
if [ -e $var ]
then
  echo "Directory: $var still exists"
  testPassed=false
fi

#check that d has also left from the root directory, should be 5 '.' , '..', a, b, and c
#Directories a,b, and c should still exist
num=$(ls -a $s | wc -l)
if [ $num -ne 5 ]
then
  echo "Not the right number of directories in the system after removing $var"
  testPassed=false
fi

var=$s"a"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi
var=$s"b"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi
var=$s"c"
if [ ! -e $var ]
then
  echo "Directory: $var should exist"
  testPassed=false
fi


if [ $testPassed == "true" ]
then
  echo "All tests Passed"
else
  echo "Tests did not pass"
fi
