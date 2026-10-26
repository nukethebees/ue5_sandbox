foreach(runtime_dll IN LISTS runtime_dlls)
  get_filename_component(runtime_name "${runtime_dll}" NAME)
  file(COPY_FILE "${runtime_dll}" "${destination}/${runtime_name}" ONLY_IF_DIFFERENT)
endforeach()
