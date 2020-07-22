# check if bash completion is available

if(DEFINED ENV{BASH_COMPLETION_DIR})
  install(FILE bash-completion.sh DESTINATION "$ENV{BASH_COMPLETION_DIR}" RENAME macho-tool)
endif()
