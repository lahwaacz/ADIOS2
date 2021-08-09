/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * MhsWriter.tcc implementation of template functions with known type
 *
 *  Created on: Apr 6, 2019
 *      Author: Jason Wang w4g@ornl.gov
 */

#ifndef ADIOS2_ENGINE_MHSWRITER_TCC_
#define ADIOS2_ENGINE_MHSWRITER_TCC_

#include "MhsWriter.h"
#include "adios2/operator/compress/CompressSirius.h"

#ifdef ADIOS2_HAVE_BLOSC
#include "adios2/operator/compress/CompressBlosc.h"
#endif

#ifdef ADIOS2_HAVE_BZIP2
#include "adios2/operator/compress/CompressBZIP2.h"
#endif

#ifdef ADIOS2_HAVE_ZFP
#include "adios2/operator/compress/CompressZFP.h"
#endif

#ifdef ADIOS2_HAVE_SZ
#include "adios2/operator/compress/CompressSZ.h"
#endif

namespace adios2
{
namespace core
{
namespace engine
{

template <>
void MhsWriter::PutDeferredCommon<std::string>(Variable<std::string> &variable,
                                               const std::string *data)
{
    auto var = m_SubIOs[0]->InquireVariable<std::string>(variable.m_Name);
    if (!var)
    {
        var = &m_SubIOs[0]->DefineVariable<std::string>(variable.m_Name,
                                                        {LocalValueDim});
    }
    m_SubEngines[0]->Put(variable, data, Mode::Sync);
}

template <class T>
void MhsWriter::PutSyncCommon(Variable<T> &variable, const T *data)
{
    PutDeferredCommon(variable, data);
    PerformPuts();
}

template <class T>
void MhsWriter::PutDeferredCommon(Variable<T> &variable, const T *data)
{
    bool putToAll = false;
    auto itVar = m_OperatorMap.find(variable.m_Name);
    if (itVar != m_OperatorMap.end())
    {
        if (itVar->second->m_Type == "sirius")
        {
            putToAll = true;
        }
    }

    auto var0 = m_SubIOs[0]->InquireVariable<T>(variable.m_Name);
    if (!var0)
    {
        var0 =
            &m_SubIOs[0]->DefineVariable<T>(variable.m_Name, variable.m_Shape);
        itVar = m_OperatorMap.find(variable.m_Name);
        if (itVar != m_OperatorMap.end())
        {
            var0->AddOperation(*itVar->second, {});
        }
    }

    var0->SetSelection({variable.m_Start, variable.m_Count});
    m_SubEngines[0]->Put(*var0, data, Mode::Sync);

    if (putToAll)
    {
        for (size_t i = 1; i < m_SubEngines.size(); ++i)
        {
            auto var = m_SubIOs[i]->InquireVariable<T>(variable.m_Name);
            if (!var)
            {
                var = &m_SubIOs[i]->DefineVariable<T>(variable.m_Name,
                                                      variable.m_Shape);
                var->AddOperation(*itVar->second, {});
            }
            var->SetSelection({variable.m_Start, variable.m_Count});
            m_SubEngines[i]->Put(*var, data, Mode::Sync);
        }
    }
}

} // end namespace engine
} // end namespace core
} // end namespace adios2

#endif /* ADIOS2_ENGINE_MHSWRITER_TCC_ */
