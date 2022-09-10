include_guard()

option( ENABLE_LLVM "Enable LLVM" ON )
message( STATUS "LLVM enabled: ${ENABLE_LLVM}" )

include( ${CMAKE_CURRENT_LIST_DIR}/CoreLib.srcs.cmake )

add_library( CoreLib STATIC ${SOURCES} )

target_include_directories(
    CoreLib
    PUBLIC  ${CMAKE_CURRENT_LIST_DIR}/Include
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/Source
)

if( ENABLE_LLVM )
    # Prerequisites:
    # - install Clang / LLVM using Homebrew
    #       $ brew install llvm
    #       $ export PATH="$(brew --prefix llvm)/bin:${PATH}"

    find_package( LLVM REQUIRED CONFIG )
    message( STATUS "LLVM version: ${LLVM_PACKAGE_VERSION}" )

    target_include_directories( CoreLib AFTER PRIVATE ${LLVM_INCLUDE_DIRS} )
    add_definitions( ${LLVM_DEFINITIONS} )

    llvm_map_components_to_libnames( LLVMLibs
        core
        irreader
        support
    )

    target_link_libraries( CoreLib PRIVATE ${LLVMLibs} )
endif()
