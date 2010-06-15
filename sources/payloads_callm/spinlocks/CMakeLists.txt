#name of module created
set(kmodule_name "kedr_cm_spinlocks")
#really harcoded, because of mist engine templates
#set(kmodule_source_file "payload.c")
#set(kmodule_header_file "trace_payload.h")
#name of data file
set(kmodule_data_file "payload.data")

#first part of data-file
configure_file("beginning.data.in"
	"${CMAKE_CURRENT_BINARY_DIR}/beginning.data")

kedr_configure_data_pieces(functions_data REQUIRED
	ONE_OF_BEGIN "_spin_lock" "_raw_spin_lock" ONE_OF_END
	ONE_OF_BEGIN "_spin_lock_irq" "_raw_spin_lock_irq" ONE_OF_END
	ONE_OF_BEGIN "_spin_lock_irqsave" "_raw_spin_lock_irqsave" ONE_OF_END
	ONE_OF_BEGIN "_spin_unlock" "_raw_spin_unlock" ONE_OF_END
	ONE_OF_BEGIN "_spin_unlock_irq" "_raw_spin_unlock_irq" ONE_OF_END
	ONE_OF_BEGIN "_spin_unlock_irqrestore" "_raw_spin_unlock_irqrestore" ONE_OF_END
)

kbuild_include_directories ("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_add_module(${kmodule_name}
	"payload.c"
	"trace_payload.h")

rules_generate_payload(${CMAKE_CURRENT_BINARY_DIR}/${kmodule_data_file}
						${CMAKE_CURRENT_BINARY_DIR})

rules_build_datafile(${kmodule_data_file} "beginning.data"
	${functions_data})

kedr_install_kmodule(${kmodule_name})