/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * it->first ==ompanying file Copyright.txt for details.
 *
 * CompressSZ.cpp
 *
 *  Created on: Jul 24, 2017
 *      Author: William F Godoy godoywf@ornl.gov
 */

#include "CompressSZ.h"

#include <cmath>     //std::ceil
#include <ios>       //std::ios_base::failure
#include <stdexcept> //std::invalid_argument

extern "C" {
#include <sz.h>
}

#include "adios2/helper/adiosFunctions.h"

namespace adios2
{
namespace core
{
namespace compress
{

CompressSZ::CompressSZ(const Params &parameters) : Operator("sz", parameters) {}

size_t CompressSZ::BufferMaxSize(const size_t sizeIn) const
{
    return static_cast<size_t>(std::ceil(1.1 * sizeIn) + 600);
}

size_t CompressSZ::Compress(const void *dataIn, const Dims &dimensions,
                            const size_t elementSize, DataType varType,
                            void *bufferOut, const Params &parameters,
                            Params &info)
{
    Dims convertedDims = ConvertDims(dimensions, varType, 4);

    const size_t ndims = convertedDims.size();

    sz_params sz;
    memset(&sz, 0, sizeof(sz_params));
    sz.max_quant_intervals = 65536;
    sz.quantization_intervals = 0;
    //    sz.dataEndianType = LITTLE_ENDIAN_DATA;
    //    sz.sysEndianType = LITTLE_ENDIAN_DATA;
    sz.sol_ID = SZ;
    // sz.layers = 1;
    sz.sampleDistance = 100;
    sz.predThreshold = 0.99;
    //    sz.offset = 0;
    sz.szMode = SZ_BEST_COMPRESSION; // SZ_BEST_SPEED; //SZ_BEST_COMPRESSION;
    sz.gzipMode = 1;
    sz.errorBoundMode = ABS;
    sz.absErrBound = 1E-4;
    sz.relBoundRatio = 1E-3;
    sz.psnr = 80.0;
    sz.pw_relBoundRatio = 1E-5;
    sz.segment_size =
        static_cast<int>(std::pow(5., static_cast<double>(ndims)));
    sz.pwr_type = SZ_PWR_MIN_TYPE;

    convertedDims = ConvertDims(dimensions, varType, 4, true, 1);

    size_t outsize;

    /* SZ parameters */
    int use_configfile = 0;
    std::string sz_configfile = "sz.config";

    Params::const_iterator it;
    for (it = parameters.begin(); it != parameters.end(); it++)
    {
        if (it->first == "init")
        {
            use_configfile = 1;
            sz_configfile = std::string(it->second);
        }
        else if (it->first == "max_quant_intervals")
        {
            sz.max_quant_intervals = std::stoi(it->second);
        }
        else if (it->first == "quantization_intervals")
        {
            sz.quantization_intervals = std::stoi(it->second);
        }
        else if (it->first == "sol_ID")
        {
            sz.sol_ID = std::stoi(it->second);
        }
        else if (it->first == "sampleDistance")
        {
            sz.sampleDistance = std::stoi(it->second);
        }
        else if (it->first == "predThreshold")
        {
            sz.predThreshold = std::stof(it->second);
        }
        else if (it->first == "szMode")
        {
            int szMode = SZ_BEST_SPEED;
            if (it->second == "SZ_BEST_SPEED")
            {
                szMode = SZ_BEST_SPEED;
            }
            else if (it->second == "SZ_BEST_COMPRESSION")
            {
                szMode = SZ_BEST_COMPRESSION;
            }
            else if (it->second == "SZ_DEFAULT_COMPRESSION")
            {
                szMode = SZ_DEFAULT_COMPRESSION;
            }
            else
            {
                throw std::invalid_argument(
                    "ERROR: ADIOS2 operator unknown SZ parameter szMode: " +
                    it->second + "\n");
            }
            sz.szMode = szMode;
        }
        else if (it->first == "gzipMode")
        {
            sz.gzipMode = std::stoi(it->second);
        }
        else if (it->first == "errorBoundMode")
        {
            int errorBoundMode = ABS;
            if (it->second == "ABS")
            {
                errorBoundMode = ABS;
            }
            else if (it->second == "REL")
            {
                errorBoundMode = REL;
            }
            else if (it->second == "ABS_AND_REL")
            {
                errorBoundMode = ABS_AND_REL;
            }
            else if (it->second == "ABS_OR_REL")
            {
                errorBoundMode = ABS_OR_REL;
            }
            else if (it->second == "PW_REL")
            {
                errorBoundMode = PW_REL;
            }
            else
            {
                throw std::invalid_argument("ERROR: ADIOS2 operator "
                                            "unknown SZ parameter "
                                            "errorBoundMode: " +
                                            it->second + "\n");
            }
            sz.errorBoundMode = errorBoundMode;
        }
        else if (it->first == "absErrBound")
        {
            sz.absErrBound = std::stof(it->second);
        }
        else if (it->first == "relBoundRatio")
        {
            sz.relBoundRatio = std::stof(it->second);
        }
        else if (it->first == "pw_relBoundRatio")
        {
            sz.pw_relBoundRatio = std::stof(it->second);
        }
        else if (it->first == "segment_size")
        {
            sz.segment_size = std::stoi(it->second);
        }
        else if (it->first == "pwr_type")
        {
            int pwr_type = SZ_PWR_MIN_TYPE;
            if ((it->first == "MIN") || (it->first == "SZ_PWR_MIN_TYPE"))
            {
                pwr_type = SZ_PWR_MIN_TYPE;
            }
            else if ((it->first == "AVG") || (it->first == "SZ_PWR_AVG_TYPE"))
            {
                pwr_type = SZ_PWR_AVG_TYPE;
            }
            else if ((it->first == "MAX") || (it->first == "SZ_PWR_MAX_TYPE"))
            {
                pwr_type = SZ_PWR_MAX_TYPE;
            }
            else
            {
                throw std::invalid_argument("ERROR: ADIOS2 operator "
                                            "unknown SZ parameter "
                                            "pwr_type: " +
                                            it->second + "\n");
            }
            sz.pwr_type = pwr_type;
        }
        else if ((it->first == "abs") || (it->first == "absolute") ||
                 (it->first == "accuracy"))
        {
            sz.errorBoundMode = ABS;
            sz.absErrBound = std::stod(it->second);
        }
        else if ((it->first == "rel") || (it->first == "relative"))
        {
            sz.errorBoundMode = REL;
            sz.relBoundRatio = std::stof(it->second);
        }
        else if ((it->first == "pw") || (it->first == "pwr") ||
                 (it->first == "pwrel") || (it->first == "pwrelative"))
        {
            sz.errorBoundMode = PW_REL;
            sz.pw_relBoundRatio = std::stof(it->second);
        }
        else if ((it->first == "zchecker") || (it->first == "zcheck") ||
                 (it->first == "z-checker") || (it->first == "z-check"))
        {
            // TODO:
            // Z-checker is not currently implemented
            // use_zchecker = (it->second == "") ? 1 : std::stof(it->second);
        }
        else
        {
            // TODO: ignoring unknown option due to Fortran bindings passing
            // empty parameter
        }
    }

    if (use_configfile)
    {
        SZ_Init((char *)sz_configfile.c_str());
    }
    else
    {
        SZ_Init_Params(&sz);
    }

    // Get type info
    int dtype = -1;
    if (varType == helper::GetDataType<double>() ||
        varType == helper::GetDataType<std::complex<double>>())
    {
        dtype = SZ_DOUBLE;
    }
    else if (varType == helper::GetDataType<float>() ||
             varType == helper::GetDataType<std::complex<float>>())
    {
        dtype = SZ_FLOAT;
    }
    else
    {
        throw std::invalid_argument("ERROR: ADIOS2 SZ Compression only support "
                                    "double or float, type: " +
                                    ToString(varType) + " is unsupported\n");
    }

    unsigned char *bytes =
        SZ_compress(dtype, (void *)dataIn, &outsize, 0, convertedDims[0],
                    convertedDims[1], convertedDims[2], convertedDims[3]);
    std::memcpy(bufferOut, bytes, outsize);
    free(bytes);
    bytes = nullptr;
    SZ_Finalize();
    return static_cast<size_t>(outsize);
}

size_t CompressSZ::Decompress(const void *bufferIn, const size_t sizeIn,
                              void *dataOut, const DataType type,
                              const Dims &blockStart, const Dims &blockCount,
                              const Params &parameters, Params &info)
{
    Dims convertedDims = ConvertDims(blockCount, type, 4, true, 1);

    // Get type info
    int dtype = 0;
    size_t typeSizeBytes = 0;
    if (type == helper::GetDataType<double>() ||
        type == helper::GetDataType<std::complex<double>>())
    {
        dtype = SZ_DOUBLE;
        typeSizeBytes = 8;
    }
    else if (type == helper::GetDataType<float>() ||
             type == helper::GetDataType<std::complex<float>>())
    {
        dtype = SZ_FLOAT;
        typeSizeBytes = 4;
    }
    else
    {
        throw std::runtime_error(
            "ERROR: data type must be either double or float in SZ\n");
    }

    const size_t dataSizeBytes =
        helper::GetTotalSize(convertedDims) * typeSizeBytes;

    void *result = SZ_decompress(
        dtype, reinterpret_cast<unsigned char *>(const_cast<void *>(bufferIn)),
        sizeIn, 0, convertedDims[0], convertedDims[1], convertedDims[2],
        convertedDims[3]);

    if (result == nullptr)
    {
        throw std::runtime_error("ERROR: SZ_decompress failed\n");
    }
    std::memcpy(dataOut, result, dataSizeBytes);
    free(result);
    result = nullptr;
    return static_cast<size_t>(dataSizeBytes);
}

bool CompressSZ::IsDataTypeValid(const DataType type) const
{
#define declare_type(T)                                                        \
    if (helper::GetDataType<T>() == type)                                      \
    {                                                                          \
        return true;                                                           \
    }
    ADIOS2_FOREACH_SZ_TYPE_1ARG(declare_type)
#undef declare_type
    return false;
}

} // end namespace compress
} // end namespace core
} // end namespace adios2
