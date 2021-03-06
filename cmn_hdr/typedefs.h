/*******************************************************************************

    Copyright 2012, 2021 Ben Wojtowicz

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*******************************************************************************

    File: typedefs.h

    Description: Contains all the common typedefs for all source code.

    Revision History
    ----------    -------------    --------------------------------------------
    02/02/2012    Ben Wojtowicz    Created file.
    08/12/2012    Ben Wojtowicz    Added 64 bit typedefs.
    02/14/2021    Ben Wojtowicz    Added complex and complex_polar.

*******************************************************************************/

#ifndef __TYPEDEFS_H__
#define __TYPEDEFS_H__

/*******************************************************************************
                              INCLUDES
*******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <complex>

/*******************************************************************************
                              DEFINES
*******************************************************************************/


/*******************************************************************************
                              TYPEDEFS
*******************************************************************************/

typedef char                int8;
typedef unsigned char       uint8;
typedef short               int16;
typedef unsigned short      uint16;
typedef int                 int32;
typedef unsigned int        uint32;
typedef long long           int64;
typedef unsigned long long  uint64;
typedef std::complex<float> complex;

#define complex_polar std::polar<float>

/*******************************************************************************
                              DECLARATIONS
*******************************************************************************/

#endif /* __TYPEDEFS_H__ */
