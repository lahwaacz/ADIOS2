/*
 * POSIXMPI.h
 *
 *  Created on: Oct 6, 2016
 *      Author: wfg
 */

#ifndef POSIX_H_
#define POSIX_H_

/// \cond EXCLUDE_FROM_DOXYGEN
/// \endcond

#include <stdio.h> //FILE*

#include "core/Transport.h"


namespace adios
{


class POSIX : public Transport
{

public:

    POSIX( MPI_Comm mpiComm, const bool debugMode, const std::vector<std::string>& arguments  );

    ~POSIX( );

    void Open( const std::string streamName, const std::string accessMode );

    void SetBuffer( std::vector<char>& buffer );

    void Write( const Capsule& capsule );

    void Close( const Capsule& capsule );


private:

    FILE* m_File; ///< POSIX C file pointer

    void Init( const std::vector<std::string>& arguments );

};


} //end namespace


#endif /* POSIX_H_ */
