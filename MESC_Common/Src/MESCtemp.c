/*
* Copyright 2021 cod3b453
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "MESCtemp.h"

#include "MESCcli.h"
#include "MESCprofile.h"

#include "conversions.h"

#include <math.h>
#include <stddef.h>

static TEMPProfile const * temp_profile = NULL;

void temp_init( TEMPProfile const * const profile )
{
    if (profile == PROFILE_DEFAULT)
    {
        static TEMPProfile temp_profile;
        uint32_t           temp_length = sizeof(temp_profile);

        ProfileStatus ret = profile_get_entry(
            "TEMP", TEMP_PROFILE_SIGNATURE,
            &temp_profile, &temp_length );

        if (ret != PROFILE_STATUS_SUCCESS)
        {
            cli_reply( "TEMP FAILED" "\r" "\n" );
            return;
        }

        temp_init( &temp_profile );
    }

    temp_profile = profile;
}

/*
Schematic

TEMP_SCHEMA_R_F_ON_R_T

     -+- V
      |
     | | R_F = 4k7
     |_|
      |
      +- Vout - >ADC
     \|
     |\| R_T
     |_\_
      |
     -+-

    R_T = Vout * R_F
          ----------
          (V - Vout)

TEMP_SCHEMA_R_T_ON_R_F

     -+- V
     \|
     |\| R_T
     |_\_
      |
      +- Vout - >ADC
      |
     | | R_F = 10k
     |_|
      |
     -+-

          V * R_F
    R_T = ------- - R_F
            Vout
*/

static float temp_calculate_R_T( float const Vout )
{
    switch (temp_profile->schema)
    {
        case TEMP_SCHEMA_R_F_ON_R_T:
        {
            float const num = (Vout * temp_profile->R_F);
            float const den = (temp_profile->V - Vout);
            float const R_T = (num / den);

            return R_T;
        }
        case TEMP_SCHEMA_R_T_ON_R_F:
        {
            float const num = (temp_profile->V * temp_profile->R_F);
            float const den = Vout;
            float const R_T = (num / den) - temp_profile->R_F;

            return R_T;
        }
        default:
        {
            // error
            return 0.0;
        }
    }
}

/*
Approximation

x(t) = t + T_lo
y(t) = ln(R) = A * x(t) + B

ln(R) - B
--------- = x(t)
     A

    ln(R) - B
T = --------- - T_lo
        A
*/

static float temp_calculate_approximation( float const R_T )
{
    float const ln_R_T = logf( R_T );
    float const T = ((ln_R_T - temp_profile->parameters.approx.B) / temp_profile->parameters.approx.A) + temp_profile->parameters.approx.Tlo;

    return T;
}

/*
Steinhart & Hart A/B/C method
*/

static void temp_derive_SteinhartHart_ABC_from_points( TEMPProfile * const profile, float const (* const R)[3], float const (* const T)[3] )
{
    float L[3];
    float Y[3];
    float g[3];

    for ( int i = 0; i < 3; i++ )
    {
        L[i] = log( (*R)[i] );
        Y[i] = (1.0f / (*T)[i]);
    }

    g[1] = (Y[1] - Y[0]) / (L[1] - L[0]);
    g[2] = (Y[2] - Y[0]) / (L[2] - L[0]);

    float const L0_2 = (L[0] * L[0]);

    profile->parameters.SH.C = ((g[2] - g[1]) / (L[2] - L[1])) * (1.0 / (L[0] + L[1] + L[2]));
    profile->parameters.SH.B = (g[1] - (profile->parameters.SH.C * (L0_2 + (L[0] * L[1]) + (L[1] * L[1]))));
    profile->parameters.SH.A = (Y[0] - (L[0] * (profile->parameters.SH.B + (profile->parameters.SH.C * L0_2))));
}

static void temp_derive_SteinhartHart_Beta_r_from_ABC( TEMPProfile * profile )
{
    profile->parameters.SH.Beta = (1.0f / profile->parameters.SH.B);
    profile->parameters.SH.r    = profile->parameters.SH.R0 * expf( -profile->parameters.SH.Beta / profile->parameters.SH.T0 );
}

static float temp_calculate_SteinhartHart_ABC( float const R_T )
{
    float const ln_R_T   = log( R_T );
    float const ln_R_T_3 = (ln_R_T * ln_R_T * ln_R_T);

    float const num = 1.0f;
    float const den = (temp_profile->parameters.SH.A + (temp_profile->parameters.SH.B * ln_R_T) + (temp_profile->parameters.SH.C * ln_R_T_3));
    float const T   = (num / den);

    return T;
}

/*
Steinhart & Hart Beta/r method
*/

static void temp_derive_SteinhartHart_ABC_from_Beta( TEMPProfile * const profile )
{
    profile->parameters.SH.C = 0.0f; // C is always zero when using Beta
    profile->parameters.SH.B = (1.0f / profile->parameters.SH.Beta);
    profile->parameters.SH.A = (profile->parameters.SH.T0 - (profile->parameters.SH.B * logf( profile->parameters.SH.R0 )));
}

static float temp_calculate_SteinhartHart_Beta_r( float const R_T )
{
    return temp_profile->parameters.SH.Beta / logf( R_T / temp_profile->parameters.SH.r );
}

/*
API
*/
//#include <stdio.h>//debug
float temp_read( uint32_t const adc_raw )
{
    float const adc  = (float)adc_raw;
    float const Vout = ((temp_profile->V * adc) / (float)temp_profile->adc_range);
    float const R_T = temp_calculate_R_T( Vout );

//fprintf( stderr, "R_T %.0f Ohm\n", R_T );//debug
    float T;

    switch (temp_profile->method)
    {
        case TEMP_METHOD_CURVE_APPROX:
        {
            T = temp_calculate_approximation( R_T );
            break;
        }
        case TEMP_METHOD_STEINHART_HART_ABC:
        {
            float const K = temp_calculate_SteinhartHart_ABC( R_T );
            T = CVT_KELVIN_TO_CELSIUS_F( K );
            break;
        }
        case TEMP_METHOD_STEINHART_HART_BETA_R:
        {
            float const K = temp_calculate_SteinhartHart_Beta_r( R_T );
            T = CVT_KELVIN_TO_CELSIUS_F( K );
            break;
        }
    }

    return T;
}

uint32_t temp_get_adc( float const T )
{
    float R_T;

    switch (temp_profile->method)
    {
        case TEMP_METHOD_CURVE_APPROX:
        {
            R_T = expf( ((T - temp_profile->parameters.approx.Tlo) * temp_profile->parameters.approx.A) + temp_profile->parameters.approx.B );
            break;
        }
        case TEMP_METHOD_STEINHART_HART_ABC:
        {
            float const K = CVT_CELSIUS_TO_KELVIN_F(T);

            float const x = ((1.0f / temp_profile->parameters.SH.C) * (temp_profile->parameters.SH.A - (1.0f / K)));
            float const Br3C = (temp_profile->parameters.SH.C / (3.0f * temp_profile->parameters.SH.C));
            float const y = sqrtf( powf( Br3C, 3.0f ) + (x * x / 4.0f) );

            float const p = (1.0f / 3.0f);
            float const xr2 = (x / 2.0f);
            float const y_m_xr2_p = powf( (y - xr2), p );
            float const y_p_xr2_p = powf( (y + xr2), p );

            R_T = expf( y_m_xr2_p - y_p_xr2_p );
            break;
        }
        case TEMP_METHOD_STEINHART_HART_BETA_R:
        {
            float const K = CVT_CELSIUS_TO_KELVIN_F( T );
            R_T = temp_profile->parameters.SH.r * expf( temp_profile->parameters.SH.Beta / K );
            // OR R_T =  R0 * exp( Beta * (1 / K - 1 / T0) )
            break;
        }
    }

    float Vout;

    switch (temp_profile->schema)
    {
        case TEMP_SCHEMA_R_F_ON_R_T:
        {
            Vout = (temp_profile->V *               R_T) / (temp_profile->R_F + R_T);
            break;
        }
        case TEMP_SCHEMA_R_T_ON_R_F:
        {
            Vout = (temp_profile->V * temp_profile->R_F) / (temp_profile->R_F + R_T);
            break;
        }
    }

    uint32_t const adc_raw = (uint32_t)((Vout * ((float)temp_profile->adc_range)) / temp_profile->V);

    return adc_raw;
}