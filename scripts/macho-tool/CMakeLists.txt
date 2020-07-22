# check if bash completion is available

if(DEFINED BASH_COMPLETION_DIR)
  install(FILES bash-completion.sh DESTINATION "$ENV{BASH_COMPLETION_DIR}" RENAME macho-tool)
endif()

