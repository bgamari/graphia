#! /bin/bash

OPTIND=1
VERBOSE=0

while getopts "vs:" OPTION
do
  case "${OPTION}" in
    v)
      VERBOSE=1
      ;;
    s)
      echo -n "Sourcing ${OPTARG}"
      if [ -e ${OPTARG} ]
      then
        echo "..."
        . ${OPTARG}
      else
        echo "...doesn't exist"
      fi
      ;;
  esac
done

shift $((OPTIND-1))
[ "$1" = "--" ] && shift

NUM_CORES=$(nproc --all)

if [ -z ${BUILD_DIR} ]
then
  COMPILER=$(basename ${CC} | sed -e 's/-.*//g')
  BUILD_DIR="build/${COMPILER}"
fi

if [ ! -z "$@" ]
then
  CPP_FILES=$@
else
  CPP_FILES=$(cat ${BUILD_DIR}/compile_commands.json | \
    jq '.[].file' | grep -vE "qrc_|mocs_compilation|thirdparty" | \
    sed -e 's/"//g')

  INCLUDE_DIRS=$(cat ${BUILD_DIR}/compile_commands.json | \
    jq '.[].command' | grep -oP '(?<=-I) *.*?(?= )' | \
    grep -vE "thirdparty|autogen" | \
    sort | uniq | \
    sed -e 's/\(.*\)/-I\1/')

  DEFINES=$(cat ${BUILD_DIR}/compile_commands.json | \
    jq '.[].command' | grep -oP '(?<=-D) *.*?(?= -\D)' | \
    grep -vE "SOURCE_DIR.*" | sort | uniq | \
    sed -e 's/=.*\"/="dummmyvalue"/g' |
    sed -e 's/\(.*\)/-D\1/')
fi

if [ "${VERBOSE}" != 0 ]
then
  echo "Files to be analysed:"
  echo ${CPP_FILES}
fi

# cppcheck
cppcheck --version
cppcheck -v --enable=all --xml --suppress=unusedFunction \
  --inline-suppr \
  --library=scripts/cppcheck.cfg \
  ${INCLUDE_DIRS} ${DEFINES} \
  ${CPP_FILES}

# clang-tidy
if [ "${VERBOSE}" != 0 ]
then
  echo "clang-tidy"
  clang-tidy --version
  clang-tidy -dump-config
fi

parallel -n1 -P${NUM_CORES} -q \
  clang-tidy -quiet -p ${BUILD_DIR} {} \
  ::: ${CPP_FILES}

# clazy
CHECKS="-checks=level1,\
base-class-event,\
container-inside-loop,\
global-const-char-pointer,\
implicit-casts,\
missing-typeinfo,\
qstring-allocations,\
reserve-candidates,\
connect-non-signal,\
lambda-in-connect,\
lambda-unique-connection,\
thread-with-slots,\
connect-not-normalized,\
overridden-signal,\
virtual-signal,\
incorrect-emit,\
qproperty-without-notify,\
no-rule-of-two-soft,\
no-qenums,\
no-non-pod-global-static,\
no-connect-3arg-lambda,\
no-const-signal-or-slot,\
global-const-char-pointer,\
implicit-casts,\
missing-qobject-macro,\
missing-typeinfo,\
returning-void-expression,\
virtual-call-ctor,\
assert-with-side-effects,\
detaching-member,\
thread-with-slots,\
connect-by-name,\
skipped-base-method,\
fully-qualified-moc-types,\
qhash-with-char-pointer-key,\
wrong-qevent-cast,\
static-pmf,\
empty-qstringliteral"

if [ "${VERBOSE}" != 0 ]
then
  echo "clazy"
  clazy-standalone --version
fi

parallel -n1 -P${NUM_CORES} \
  clazy-standalone -p ${BUILD_DIR}/compile_commands.json \
  -header-filter="\"^((?!thirdparty).)*$\"" \
  ${CHECKS} {} \
  ::: ${CPP_FILES}

# qmllint
qmllint --version
find source/app \
  source/shared \
  source/plugins \
  source/crashreporter \
  -type f -iname "*.qml" | \
  xargs -n1 -P${NUM_CORES} qmllint
