#!/bin/bash -e

if [ "$#" -ne 3]; then
  echo "Usage: $0 VERSION LOCAL_PATH GUI_PATH"
  echo "  VERSION is a version string such as 0.26.0-alpha"
  echo "  LOCAL_PATH is the path to the command line tools e.g. /usr/local"
  echo "  GUI_PATH is the path to the viewer App e.g. /Applications"
  exit 1
fi

SEEC_VERSION=$1    # e.g. 0.26.0~pr3
SEEC_LOCAL_PATH=$2 # e.g. /usr/local
SEEC_GUI_PATH=$3   # e.g. /Applications

WORKING_DIR=`pwd`

# c.f. http://stackoverflow.com/a/246128
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "WORKING_DIR = $WORKING_DIR"

LOCAL_UNPKG=${WORKING_DIR}/seec-local.unpkg
GUI_UNPKG=${WORKING_DIR}/seec-gui.unpkg
SEEC_UNPKG=${WORKING_DIR}/seec.unpkg

# PREPARE THE UNPACKAGED COMMAND LINE TOOLS:
rm -rf ${LOCAL_UNPKG}

mkdir ${LOCAL_UNPKG}
mkdir ${LOCAL_UNPKG}/bin
mkdir ${LOCAL_UNPKG}/lib

cp ${SEEC_LOCAL_PATH}/bin/seec-cc seec-local.unpkg/bin/
cp ${SEEC_LOCAL_PATH}/bin/seec-ld seec-local.unpkg/bin/
cp -R ${SEEC_LOCAL_PATH}/lib/seec seec-local.unpkg/lib/

# PREPARE THE UNPACKAGED TRACE VIEWER:
rm -rf seec-gui.unpkg
mkdir seec-gui.unpkg

cp -R ${SEEC_GUI_PATH}/seec-view.app seec-gui.unpkg/

# TODO: BUNDLE-DYLIBS?

# PREPARE UNPACKAGED FINAL
rm -rf ${SEEC_UNPKG}

mkdir ${SEEC_UNPKG}
mkdir ${SEEC_UNPKG}/local.pkg
mkdir ${SEEC_UNPKG}/seec-view.pkg


# PACK THE COMMAND LINE TOOLS
cd ${LOCAL_UNPKG}
find . | cpio -o --format odc | gzip -c > ${SEEC_UNPKG}/local.pkg/Payload
cd ${WORKING_DIR}
mkbom ${LOCAL_UNPKG}/ ${SEEC_UNPKG}/local.pkg/Bom

LOCAL_INSTALL_SIZE=`du -sk ${LOCAL_UNPKG}/ | cut -f1`
let LOCAL_INSTALL_SIZE-=4
LOCAL_NUM_FILES=`find ${LOCAL_UNPKG}/ | wc -l | tr -d '[[:space:]]'`

echo "LOCAL_INSTALL_SIZE = '${LOCAL_INSTALL_SIZE}'"
echo "LOCAL_NUM_FILES = '${LOCAL_NUM_FILES}'"


# PACK THE TRACE VIEWER
cd ${GUI_UNPKG}
find . | cpio -o --format odc | gzip -c > ${SEEC_UNPKG}/seec-view.pkg/Payload
cd ${WORKING_DIR}
mkbom ${GUI_UNPKG}/ ${SEEC_UNPKG}/seec-view.pkg/Bom

GUI_INSTALL_SIZE=`du -sk ${GUI_UNPKG}/ | cut -f1`
let GUI_INSTALL_SIZE-=4
GUI_NUM_FILES=`find ${GUI_UNPKG}/ | wc -l | tr -d '[[:space:]]'`

echo "GUI_INSTALL_SIZE = '${GUI_INSTALL_SIZE}'"
echo "GUI_NUM_FILES = '${GUI_NUM_FILES}'"


# PACK FINAL

RE_VERSION="s/SEEC_VERSION/${SEEC_VERSION}/g"
RE_CLI_SIZE="s/SEEC_CL_INSTALL_KBYTES/${LOCAL_INSTALL_SIZE}/g"
RE_CLI_NUM="s/SEEC_CL_NUM_FILES/${LOCAL_NUM_FILES}/g"
RE_GUI_SIZE="s/SEEC_VIEWER_INSTALL_KBYTES/${GUI_INSTALL_SIZE}/g"
RE_GUI_NUM="s/SEEC_VIEWER_NUM_FILES/${GUI_NUM_FILES}/g"

sed "${RE_VERSION};${RE_CLI_SIZE};${RE_GUI_SIZE}" \
    ${SCRIPT_DIR}/src/Distribution.template > ${SEEC_UNPKG}/Distribution

sed "${RE_VERSION};${RE_CLI_SIZE};${RE_CLI_NUM}"   \
    ${SCRIPT_DIR}/src/local.PackageInfo.template > \
    ${SEEC_UNPKG}/local.pkg/PackageInfo

sed "${RE_VERSION};${RE_GUI_SIZE};${RE_GUI_NUM}"       \
    ${SCRIPT_DIR}/src/seec-view.PackageInfo.template > \
    ${SEEC_UNPKG}/seec-view.pkg/PackageInfo

cp -R ${SCRIPT_DIR}/src/Resources ${SEEC_UNPKG}/

pkgutil --flatten ${SEEC_UNPKG}/ seec-${SEEC_VERSION}.pkg

