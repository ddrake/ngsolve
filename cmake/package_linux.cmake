execute_process(COMMAND grep CODENAME /etc/lsb-release OUTPUT_VARIABLE temp OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND dpkg --print-architecture OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
if(temp)
    set(CPACK_DEBIAN_PACKAGE_NAME ${CPACK_PACKAGE_NAME} CACHE STRING "Debian package name")
    set(CPACK_GENERATOR "DEB")
    string(SUBSTRING ${temp} 17 -1 UBUNTU_VERSION)
    message("ubuntu version: ${UBUNTU_VERSION}")

    set(CPACK_DEBIAN_PACKAGE_DEPENDS "python3, libpython3-dev, python3-tk, libxmu-dev, tk-dev, tcl-dev, libglu1-mesa-dev")
    execute_process(COMMAND dpkg --print-architecture OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Matthias Hochsteger <matthias.hochsteger@tuwien.ac.at>")
    if(USE_MPI)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libmetis5, openmpi-bin")
        set(CPACK_PACKAGE_NAME "${CPACK_PACKAGE_NAME}_mpi")
    endif(USE_MPI)
    if(USE_OCC)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, liboce-ocaf-dev")
    endif(USE_OCC)
    set(CPACK_DEBIAN_PACKAGE_SECTION Science)
    set(CPACK_DEBIAN_PACKAGE_PROVIDES "netgen, ngsolve")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
endif(temp)
