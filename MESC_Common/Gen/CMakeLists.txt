# Copyright 2021-2022 cod3b453
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

CMAKE_MINIMUM_REQUIRED( VERSION 3.10 )

PROJECT( util_ntc )

IF(MSVC)
    SET( CMAKE_C_FLAGS         "/W4 /MP" )
    SET( CMAKE_C_FLAGS_DEBUG   "/Od /GS /ZI /RTC1" )
    SET( CMAKE_C_FLAGS_RELEASE "/O2 /GF" )
ELSE()
    SET( CMAKE_C_FLAGS         "-Wall -Wextra -pedantic -std=c11" )
    SET( CMAKE_C_FLAGS_DEBUG   "-O0 -g -ggdb3" )
    SET( CMAKE_C_FLAGS_RELEASE "-O2" )
ENDIF()

SET( ${PROJECT_NAME}_inc
    ${CMAKE_CURRENT_LIST_DIR}/../Inc

    ${CMAKE_CURRENT_LIST_DIR}
)

SET( ${PROJECT_NAME}_hdr
    ${CMAKE_CURRENT_LIST_DIR}/../Inc/bit_op.h

    ${CMAKE_CURRENT_LIST_DIR}/util_ntc.h
)

SET( ${PROJECT_NAME}_src
    ${CMAKE_CURRENT_LIST_DIR}/ntc.c

    ${CMAKE_CURRENT_LIST_DIR}/util_ntc.c
)

ADD_EXECUTABLE( ${PROJECT_NAME} ${${PROJECT_NAME}_hdr} ${${PROJECT_NAME}_src} )

TARGET_INCLUDE_DIRECTORIES( ${PROJECT_NAME} PUBLIC ${${PROJECT_NAME}_inc} )

IF(MSVC)
    #SET_PROPERTY( DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME} )
ELSE()
    TARGET_LINK_LIBRARIES( ${PROJECT_NAME} PUBLIC m )
ENDIF()

PROJECT( util )
