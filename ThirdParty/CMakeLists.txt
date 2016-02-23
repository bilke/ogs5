# Disable warnings for this directory
if(MSVC)
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/w" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /w")
	endif()
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
endif()

add_subdirectory(Brent)
if(OGS_USE_CVODE)
	add_subdirectory(cvode)
	set_target_properties(sundials_cvode_shared sundials_nvecserial_shared PROPERTIES MACOSX_RPATH ON)
endif()

if(OGS_CONFIG STREQUAL IPQC)
	add_subdirectory(IPQC)
endif()

if(OGS_CONFIG STREQUAL GEMS OR OGS_CONFIG STREQUAL PETSC_GEMS)
	add_subdirectory(GEM)
endif()

if(OGS_CONFIG STREQUAL PQC)
	add_subdirectory ( PQC )
endif()
