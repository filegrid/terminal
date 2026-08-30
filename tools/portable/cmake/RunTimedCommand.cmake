if(NOT DEFINED STEP_NAME)
    message(FATAL_ERROR "RunTimedCommand.cmake requires STEP_NAME.")
endif()

set(_command)
set(_capture_command FALSE)
math(EXPR _last_argv_index "${CMAKE_ARGC} - 1")
foreach(_index RANGE 0 ${_last_argv_index})
    set(_arg "${CMAKE_ARGV${_index}}")
    if(_capture_command)
        list(APPEND _command "${_arg}")
    elseif(_arg STREQUAL "--")
        set(_capture_command TRUE)
    endif()
endforeach()

if(NOT _command)
    message(FATAL_ERROR "RunTimedCommand.cmake requires a command after --.")
endif()

string(TIMESTAMP _start_text "%Y-%m-%d %H:%M:%S")
string(TIMESTAMP _start_epoch "%s")
message("[${_start_text}] START ${STEP_NAME}")

set(_execute_process_args
    COMMAND ${_command}
    RESULT_VARIABLE _step_result
    ECHO_OUTPUT_VARIABLE
    ECHO_ERROR_VARIABLE
)
if(DEFINED STEP_WORKING_DIRECTORY AND NOT STEP_WORKING_DIRECTORY STREQUAL "")
    list(APPEND _execute_process_args WORKING_DIRECTORY "${STEP_WORKING_DIRECTORY}")
endif()

execute_process(${_execute_process_args})

string(TIMESTAMP _end_text "%Y-%m-%d %H:%M:%S")
string(TIMESTAMP _end_epoch "%s")
math(EXPR _elapsed_seconds "${_end_epoch} - ${_start_epoch}")
message("[${_end_text}] END ${STEP_NAME} exit=${_step_result} elapsed=${_elapsed_seconds}s")

if(NOT _step_result EQUAL 0)
    message(FATAL_ERROR "${STEP_NAME} failed with exit code ${_step_result}.")
endif()
