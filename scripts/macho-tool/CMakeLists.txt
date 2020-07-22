# check if bash completion is available

if(NOT DEFINED BASH_COMPLETION_DIR)
  if(DEFINED ENV{BASH_COMPLETION_DIR})
    set(BASH_COMPLETION_DIR "$ENV{BASH_COMPLETION_DIR}")
  elseif(IS_DIRECTORY "/usr/local/etc/bash_completion.d")
    set(BASH_COMPLETION_DIR "/usr/local/etc/bash_completion.d")
  endif()
endif()

if(DEFINED BASH_COMPLETION_DIR)
  install(FILES bash-completion.sh DESTINATION "$ENV{BASH_COMPLETION_DIR}" RENAME macho-tool)
endif()

