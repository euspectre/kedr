# The path where the HTML docs will be installed
set (KEDR_DOC_PATH "${KEDR_INSTALL_PREFIX_DOC}/${KEDR_PACKAGE_NAME}-${KEDR_VERSION_MAJOR}.${KEDR_VERSION_MINOR}")

# Main target to generate or just copy the documentation
add_custom_target(doc ALL)

########################################################################
# If KEDR_GENERATE_DOC is defined, HTML documentation should be generated
# from DocBook sources. Otherwise the stored version of the docs will be 
# simply copied to the build tree, ready to be installed.
########################################################################
option(KEDR_GENERATE_DOC "Whether need to generate documentation from sources." OFF)

# Directory for the documentation in HTML format in the binary tree.
set (KEDR_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")
add_custom_target (make_doc_dirs
    COMMAND ${CMAKE_COMMAND} -E make_directory "${KEDR_HTML_DIR}"
    
# Clean html directory - just in case it existed before
    COMMAND rm -rf "${KEDR_HTML_DIR}/*"
)

if (KEDR_GENERATE_DOC)
    message (STATUS 
"Checking whether to generate the documentation from DocBook sources - yes"
    )
    SET(DOCBOOK_DTD_FILE "${CMAKE_SOURCE_DIR}/../dtd/docbookx.dtd" CACHE FILEPATH "Path to DTD file for DocBook")
    SET(DOCBOOK_XSL_PATH "${CMAKE_SOURCE_DIR}/../xsl" CACHE PATH "Directory containing XSL stylesheets for DocBook")
# Check if xsltproc tool, DTD and XSL files are available.
    message (STATUS "Checking for xsltproc")
    find_program (XSLTPROC xsltproc DOC "Path to xsltproc tool.")
    if (NOT XSLTPROC)
        message (FATAL_ERROR 
"xsltproc tool is not found. It is usually provided by 'libxslt' package or something similar."
        )
    else ()
        message (STATUS "Checking for xsltproc: ${XSLTPROC}")
    endif ()

    if (NOT EXISTS "${DOCBOOK_DTD_FILE}")
        message (STATUS 
        "DocBook DTD file is missing: ${DOCBOOK_DTD_FILE}"
        )
        message (FATAL_ERROR 
        "Please make sure DOCBOOK_DTD_FILE parameter is set properly."
        )
    endif ()
    message (STATUS "DocBook DTD file is ${DOCBOOK_DTD_FILE}")
    
    if (NOT EXISTS "${DOCBOOK_XSL_PATH}/html/")
        message (STATUS 
        "No XSL stylesheets for HTML found in ${DOCBOOK_XSL_PATH}"
        )
        message (FATAL_ERROR 
        "Please make sure DOCBOOK_XSL_PATH parameter is set properly."
        )
    endif ()
    message (STATUS "DocBook XSL stylesheet path is ${DOCBOOK_XSL_PATH}")

# Directory for DocBook sources in the binary tree.
    set (KEDR_DOCBOOK_DIR "${CMAKE_CURRENT_BINARY_DIR}/docbook")
    
    configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/docbook/main.xml.cmake"
        "${KEDR_DOCBOOK_DIR}/main.xml"
    )
    
    configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/docbook/config.xsl.cmake"
        "${KEDR_DOCBOOK_DIR}/config.xsl"
    )
    
    configure_file (
        "${CMAKE_CURRENT_SOURCE_DIR}/docbook/entities.xml.cmake"
        "${KEDR_DOCBOOK_DIR}/entities.xml"
    )
  
# Copy the files needed to generate the docs to the build tree for them to
# be located in a single place.
    file (GLOB DOCBOOK_SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/docbook/*.xml")
    file (GLOB KEDR_CSS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/docbook/*.css")

# Check for missing files
    if ("${DOCBOOK_SRC_FILES}" STREQUAL "")
        message (FATAL_ERROR
"No DocBook source files (*.xml) found in ${CMAKE_CURRENT_SOURCE_DIR}/docbook/"
        )
    endif ()
    
    if ("${KEDR_CSS_FILES}" STREQUAL "")
        message (FATAL_ERROR
"No CSS files (*.css) found in ${CMAKE_CURRENT_SOURCE_DIR}/docbook/"
        )
    endif ()
    
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docbook/images")
        message (FATAL_ERROR
"'images' directory is missing from ${CMAKE_CURRENT_SOURCE_DIR}/docbook/"
        )
    endif ()
    
# [NB] ${KEDR_DOCBOOK_DIR} should have already been created - see 
# configure_file() statements above.
    
    add_custom_target (copy_doc_files
# Copy DocBook XML sources
        COMMAND cp -p ${DOCBOOK_SRC_FILES} "${KEDR_DOCBOOK_DIR}"

# Copy CSS files to both docbook and html directories 
        COMMAND cp -p ${KEDR_CSS_FILES} "${KEDR_HTML_DIR}"
        COMMAND cp -p ${KEDR_CSS_FILES} "${KEDR_DOCBOOK_DIR}"
        
# Copy images to docbook and html directories
        COMMAND cp -pr "${CMAKE_CURRENT_SOURCE_DIR}/docbook/images" 
            "${KEDR_HTML_DIR}"
        COMMAND cp -pr "${CMAKE_CURRENT_SOURCE_DIR}/docbook/images" 
            "${KEDR_DOCBOOK_DIR}/images"
    )

    add_dependencies (copy_doc_files make_doc_dirs)

# Generate the documentation in HTML format from DocBook sources   
    add_custom_target (generate_doc
        COMMAND ${XSLTPROC} --nonet -o "${KEDR_HTML_DIR}/" config.xsl main.xml
        WORKING_DIRECTORY "${KEDR_DOCBOOK_DIR}"
        COMMENT "Generating HTML documentation from DocBook sources"
    )
    add_dependencies (generate_doc copy_doc_files)

# Chain up doc generation stuff to 'doc' target.
    add_dependencies (doc generate_doc)

else () # KEDR_GENERATE_DOC is not defined.
    message (STATUS 
"Checking whether to generate the documentation from DocBook sources - no"
    )
# Do not generate the docs, just copy them to the build tree.
    
    file (GLOB DOC_HTML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/html/*.html")
    file (GLOB KEDR_CSS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/html/*.css")

# Check for missing files
    if ("${DOC_HTML_FILES}" STREQUAL "")
        message (FATAL_ERROR
"No HTML files (*.html) found in ${CMAKE_CURRENT_SOURCE_DIR}/html/"
        )
    endif ()
    
    if ("${KEDR_CSS_FILES}" STREQUAL "")
        message (FATAL_ERROR
"No CSS files (*.css) found in ${CMAKE_CURRENT_SOURCE_DIR}/html/"
        )
    endif ()
    
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/html/images")
        message (FATAL_ERROR
"'images' directory is missing from ${CMAKE_CURRENT_SOURCE_DIR}/html/"
        )
    endif ()

    add_custom_target (copy_doc_files
# Copy HTML files
        COMMAND cp -p ${DOC_HTML_FILES} "${KEDR_HTML_DIR}"

# Copy CSS files to both docbook and html directories 
        COMMAND cp -p ${KEDR_CSS_FILES} "${KEDR_HTML_DIR}"
        
# Copy images to docbook and html directories
        COMMAND cp -pr "${CMAKE_CURRENT_SOURCE_DIR}/html/images" 
            "${KEDR_HTML_DIR}"
    )

    add_dependencies (copy_doc_files make_doc_dirs)
    add_dependencies (doc copy_doc_files)
endif (KEDR_GENERATE_DOC)
#######################################################################

# Install the docs
# [NB] The trailing '/' is important. If it is present, the contents of 
# the directory are installed to the specified path rather than the directory
# itself.
install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/html/"
    DESTINATION "${KEDR_DOC_PATH}"
)

#######################################################################
