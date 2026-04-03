# Copies the Resources directory to the destination, excluding source/editor-only files.
# Usage: cmake -DSRC=<src> -DDST=<dst> -P CopyResources.cmake
file(COPY "${SRC}/" DESTINATION "${DST}")
