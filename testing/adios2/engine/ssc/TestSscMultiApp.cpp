/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 */

#include <adios2.h>
#include <gtest/gtest.h>
#ifdef ADIOS2_HAVE_MPI
#include <mpi.h>
#endif
#include <numeric>
#include <thread>

using namespace adios2;
int mpiRank = 0;
int mpiSize = 1;
int mpiGroup;
MPI_Comm mpiComm;
size_t print_lines = 0;

char runMode;

class SscEngineTest : public ::testing::Test
{
public:
    SscEngineTest() = default;
};

template <class T>
void PrintData(const T *data, const size_t step, const Dims &start,
               const Dims &count)
{
    size_t size = std::accumulate(count.begin(), count.end(), 1,
                                  std::multiplies<size_t>());
    std::cout << "Rank: " << mpiRank << " Step: " << step << " Size:" << size
              << "\n";
    size_t printsize = 128;

    if (size < printsize)
    {
        printsize = size;
    }
    int s = 0;
    for (size_t i = 0; i < printsize; ++i)
    {
        ++s;
        std::cout << data[i] << " ";
        if (s == count[1])
        {
            std::cout << std::endl;
            s = 0;
        }
    }

    std::cout << "]" << std::endl;
}

template <class T>
void GenDataRecursive(std::vector<size_t> start, std::vector<size_t> count,
                      std::vector<size_t> shape, size_t n0, size_t y,
                      std::vector<T> &vec)
{
    for (size_t i = 0; i < count[0]; i++)
    {
        size_t i0 = n0 * count[0] + i;
        size_t z = y * shape[0] + (i + start[0]);

        auto start_next = start;
        auto count_next = count;
        auto shape_next = shape;
        start_next.erase(start_next.begin());
        count_next.erase(count_next.begin());
        shape_next.erase(shape_next.begin());

        if (start_next.size() == 1)
        {
            for (size_t j = 0; j < count_next[0]; j++)
            {
                vec[i0 * count_next[0] + j] =
                    z * shape_next[0] + (j + start_next[0]);
            }
        }
        else
        {
            GenDataRecursive(start_next, count_next, shape_next, i0, z, vec);
        }
    }
}

template <class T>
void GenData(std::vector<T> &vec, const size_t step,
             const std::vector<size_t> &start, const std::vector<size_t> &count,
             const std::vector<size_t> &shape)
{
    size_t total_size = std::accumulate(count.begin(), count.end(), 1,
                                        std::multiplies<size_t>());
    vec.resize(total_size);
    GenDataRecursive(start, count, shape, 0, 0, vec);
}

template <class T>
void VerifyData(const std::complex<T> *data, size_t step, const Dims &start,
                const Dims &count, const Dims &shape)
{
    size_t size = std::accumulate(count.begin(), count.end(), 1,
                                  std::multiplies<size_t>());
    std::vector<std::complex<T>> tmpdata(size);
    GenData(tmpdata, step, start, count, shape);
    for (size_t i = 0; i < size; ++i)
    {
        ASSERT_EQ(data[i], tmpdata[i]);
    }
    if (print_lines < 32)
    {
        PrintData(data, step, start, count);
        ++print_lines;
    }
}

template <class T>
void VerifyData(const T *data, size_t step, const Dims &start,
                const Dims &count, const Dims &shape)
{
    size_t size = std::accumulate(count.begin(), count.end(), 1,
                                  std::multiplies<size_t>());
    bool compressed = false;
    std::vector<T> tmpdata(size);
    if (print_lines < 32)
    {
        PrintData(data, step, start, count);
        ++print_lines;
    }
    GenData(tmpdata, step, start, count, shape);
    for (size_t i = 0; i < size; ++i)
    {
        if (!compressed)
        {
            ASSERT_EQ(data[i], tmpdata[i]);
        }
    }
}

void Writer1(const Dims &shape, const Dims &start, const Dims &count,
             const size_t steps, const adios2::Params &engineParams,
             const std::string &name)
{
    size_t datasize = std::accumulate(count.begin(), count.end(), 1,
                                      std::multiplies<size_t>());
    adios2::ADIOS adios(mpiComm, adios2::DebugON);
    adios2::IO dataManIO = adios.DeclareIO("WAN");
    dataManIO.SetEngine("ssc");
    dataManIO.SetParameters(engineParams);
    std::vector<char> myChars(datasize);
    std::vector<unsigned char> myUChars(datasize);
    std::vector<short> myShorts(datasize);
    std::vector<unsigned short> myUShorts(datasize);
    std::vector<int> myInts(datasize);
    std::vector<unsigned int> myUInts(datasize);
    std::vector<float> myFloats(datasize);
    std::vector<double> myDoubles(datasize);
    std::vector<std::complex<float>> myComplexes(datasize);
    std::vector<std::complex<double>> myDComplexes(datasize);
    auto bpChars =
        dataManIO.DefineVariable<char>("bpChars", shape, start, count);
    auto bpUChars = dataManIO.DefineVariable<unsigned char>("bpUChars", shape,
                                                            start, count);
    auto bpShorts =
        dataManIO.DefineVariable<short>("bpShorts", shape, start, count);
    auto bpUShorts = dataManIO.DefineVariable<unsigned short>(
        "bpUShorts", shape, start, count);
    auto bpInts = dataManIO.DefineVariable<int>("bpInts", shape, start, count);
    auto bpUInts =
        dataManIO.DefineVariable<unsigned int>("bpUInts", shape, start, count);
    auto bpFloats =
        dataManIO.DefineVariable<float>("bpFloats", shape, start, count);
    auto bpDoubles =
        dataManIO.DefineVariable<double>("bpDoubles", shape, start, count);
    auto bpComplexes = dataManIO.DefineVariable<std::complex<float>>(
        "bpComplexes", shape, start, count);
    auto bpDComplexes = dataManIO.DefineVariable<std::complex<double>>(
        "bpDComplexes", shape, start, count);
    dataManIO.DefineAttribute<int>("AttInt", 110);
    adios2::Engine dataManWriter = dataManIO.Open(name, adios2::Mode::Write);
    for (int i = 0; i < steps; ++i)
    {
        dataManWriter.BeginStep();
        GenData(myChars, i, start, count, shape);
        GenData(myUChars, i, start, count, shape);
        GenData(myShorts, i, start, count, shape);
        GenData(myUShorts, i, start, count, shape);
        GenData(myInts, i, start, count, shape);
        GenData(myUInts, i, start, count, shape);
        GenData(myFloats, i, start, count, shape);
        GenData(myDoubles, i, start, count, shape);
        GenData(myComplexes, i, start, count, shape);
        GenData(myDComplexes, i, start, count, shape);
        dataManWriter.Put(bpChars, myChars.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpUChars, myUChars.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpShorts, myShorts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpUShorts, myUShorts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpInts, myInts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpUInts, myUInts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpFloats, myFloats.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpDoubles, myDoubles.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpComplexes, myComplexes.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpDComplexes, myDComplexes.data(),
                          adios2::Mode::Sync);
        dataManWriter.EndStep();
    }
    dataManWriter.Close();
}

void Writer2(const Dims &shape, const Dims &start, const Dims &count,
             const size_t steps, const adios2::Params &engineParams,
             const std::string &name)
{
    size_t datasize = std::accumulate(count.begin(), count.end(), 1,
                                      std::multiplies<size_t>());
    adios2::ADIOS adios(mpiComm, adios2::DebugON);
    adios2::IO dataManIO = adios.DeclareIO("WAN");
    dataManIO.SetEngine("ssc");
    dataManIO.SetParameters(engineParams);
    std::vector<char> myChars(datasize);
    std::vector<unsigned char> myUChars(datasize);
    std::vector<short> myShorts(datasize);
    std::vector<unsigned short> myUShorts(datasize);
    std::vector<int> myInts(datasize);
    std::vector<unsigned int> myUInts(datasize);
    std::vector<float> myFloats(datasize);
    std::vector<double> myDoubles(datasize);
    std::vector<std::complex<float>> myComplexes(datasize);
    std::vector<std::complex<double>> myDComplexes(datasize);
    auto bpChars =
        dataManIO.DefineVariable<char>("bpChars2", shape, start, count);
    auto bpUChars = dataManIO.DefineVariable<unsigned char>("bpUChars2", shape,
                                                            start, count);
    auto bpShorts =
        dataManIO.DefineVariable<short>("bpShorts2", shape, start, count);
    auto bpUShorts = dataManIO.DefineVariable<unsigned short>(
        "bpUShorts2", shape, start, count);
    auto bpInts = dataManIO.DefineVariable<int>("bpInts2", shape, start, count);
    auto bpUInts =
        dataManIO.DefineVariable<unsigned int>("bpUInts2", shape, start, count);
    auto bpFloats =
        dataManIO.DefineVariable<float>("bpFloats2", shape, start, count);
    auto bpDoubles =
        dataManIO.DefineVariable<double>("bpDoubles2", shape, start, count);
    auto bpComplexes = dataManIO.DefineVariable<std::complex<float>>(
        "bpComplexes2", shape, start, count);
    auto bpDComplexes = dataManIO.DefineVariable<std::complex<double>>(
        "bpDComplexes2", shape, start, count);
    dataManIO.DefineAttribute<int>("AttInt2", 111);
    adios2::Engine dataManWriter = dataManIO.Open(name, adios2::Mode::Write);
    for (int i = 0; i < steps; ++i)
    {
        dataManWriter.BeginStep();
        GenData(myChars, i, start, count, shape);
        GenData(myUChars, i, start, count, shape);
        GenData(myShorts, i, start, count, shape);
        GenData(myUShorts, i, start, count, shape);
        GenData(myInts, i, start, count, shape);
        GenData(myUInts, i, start, count, shape);
        GenData(myFloats, i, start, count, shape);
        GenData(myDoubles, i, start, count, shape);
        GenData(myComplexes, i, start, count, shape);
        GenData(myDComplexes, i, start, count, shape);
        dataManWriter.Put(bpChars, myChars.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpUChars, myUChars.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpShorts, myShorts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpUShorts, myUShorts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpInts, myInts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpUInts, myUInts.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpFloats, myFloats.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpDoubles, myDoubles.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpComplexes, myComplexes.data(), adios2::Mode::Sync);
        dataManWriter.Put(bpDComplexes, myDComplexes.data(),
                          adios2::Mode::Sync);
        dataManWriter.EndStep();
    }
    dataManWriter.Close();
}

void Reader1(const Dims &shape, const Dims &start, const Dims &count,
             const size_t steps, const adios2::Params &engineParams,
             const std::string &name)
{
    adios2::ADIOS adios(mpiComm, adios2::DebugON);
    adios2::IO dataManIO = adios.DeclareIO("Test");
    dataManIO.SetEngine("ssc");
    dataManIO.SetParameters(engineParams);
    adios2::Engine dataManReader = dataManIO.Open(name, adios2::Mode::Read);

    size_t datasize = std::accumulate(shape.begin(), shape.end(), 1,
                                      std::multiplies<size_t>());
    std::vector<char> myChars(datasize);
    std::vector<unsigned char> myUChars(datasize);
    std::vector<short> myShorts(datasize);
    std::vector<unsigned short> myUShorts(datasize);
    std::vector<int> myInts(datasize);
    std::vector<unsigned int> myUInts(datasize);
    std::vector<float> myFloats(datasize);
    std::vector<double> myDoubles(datasize);
    std::vector<std::complex<float>> myComplexes(datasize);
    std::vector<std::complex<double>> myDComplexes(datasize);

    while (true)
    {
        adios2::StepStatus status = dataManReader.BeginStep(StepMode::Read, 5);
        if (status == adios2::StepStatus::OK)
        {
            const auto &vars = dataManIO.AvailableVariables();
            if (print_lines == 0)
            {
                std::cout << "All available variables : ";
                for (const auto &var : vars)
                {
                    std::cout << var.first << ", ";
                }
                std::cout << std::endl;
            }
            ASSERT_EQ(vars.size(), 20);
            size_t currentStep = dataManReader.CurrentStep();
            adios2::Variable<char> bpChars =
                dataManIO.InquireVariable<char>("bpChars");
            adios2::Variable<unsigned char> bpUChars =
                dataManIO.InquireVariable<unsigned char>("bpUChars");
            adios2::Variable<short> bpShorts =
                dataManIO.InquireVariable<short>("bpShorts");
            adios2::Variable<unsigned short> bpUShorts =
                dataManIO.InquireVariable<unsigned short>("bpUShorts");
            adios2::Variable<int> bpInts =
                dataManIO.InquireVariable<int>("bpInts");
            adios2::Variable<unsigned int> bpUInts =
                dataManIO.InquireVariable<unsigned int>("bpUInts2");
            adios2::Variable<float> bpFloats =
                dataManIO.InquireVariable<float>("bpFloats2");
            adios2::Variable<double> bpDoubles =
                dataManIO.InquireVariable<double>("bpDoubles2");
            adios2::Variable<std::complex<float>> bpComplexes =
                dataManIO.InquireVariable<std::complex<float>>("bpComplexes2");
            adios2::Variable<std::complex<double>> bpDComplexes =
                dataManIO.InquireVariable<std::complex<double>>(
                    "bpDComplexes2");
            auto charsBlocksInfo = dataManReader.AllStepsBlocksInfo(bpChars);

            dataManReader.Get(bpChars, myChars.data(), adios2::Mode::Sync);
            dataManReader.Get(bpUChars, myUChars.data(), adios2::Mode::Sync);
            dataManReader.Get(bpShorts, myShorts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpUShorts, myUShorts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpInts, myInts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpUInts, myUInts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpFloats, myFloats.data(), adios2::Mode::Sync);
            dataManReader.Get(bpDoubles, myDoubles.data(), adios2::Mode::Sync);
            dataManReader.Get(bpComplexes, myComplexes.data(),
                              adios2::Mode::Sync);
            dataManReader.Get(bpDComplexes, myDComplexes.data(),
                              adios2::Mode::Sync);
            VerifyData(myChars.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myUChars.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myShorts.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myUShorts.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myInts.data(), currentStep, Dims(shape.size(), 0), shape,
                       shape);
            VerifyData(myUInts.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myFloats.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myDoubles.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myComplexes.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myDComplexes.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            dataManReader.EndStep();
        }
        else if (status == adios2::StepStatus::EndOfStream)
        {
            std::cout << "[Rank " + std::to_string(mpiRank) +
                             "] SscTest reader end of stream!"
                      << std::endl;
            break;
        }
    }
    dataManReader.Close();
    print_lines = 0;
}

void Reader2(const Dims &shape, const Dims &start, const Dims &count,
             const size_t steps, const adios2::Params &engineParams,
             const std::string &name)
{
    adios2::ADIOS adios(mpiComm, adios2::DebugON);
    adios2::IO dataManIO = adios.DeclareIO("Test");
    dataManIO.SetEngine("ssc");
    dataManIO.SetParameters(engineParams);
    adios2::Engine dataManReader = dataManIO.Open(name, adios2::Mode::Read);

    size_t datasize = std::accumulate(shape.begin(), shape.end(), 1,
                                      std::multiplies<size_t>());
    std::vector<char> myChars(datasize);
    std::vector<unsigned char> myUChars(datasize);
    std::vector<short> myShorts(datasize);
    std::vector<unsigned short> myUShorts(datasize);
    std::vector<int> myInts(datasize);
    std::vector<unsigned int> myUInts(datasize);
    std::vector<float> myFloats(datasize);
    std::vector<double> myDoubles(datasize);
    std::vector<std::complex<float>> myComplexes(datasize);
    std::vector<std::complex<double>> myDComplexes(datasize);

    while (true)
    {
        adios2::StepStatus status = dataManReader.BeginStep(StepMode::Read, 5);
        if (status == adios2::StepStatus::OK)
        {
            const auto &vars = dataManIO.AvailableVariables();
            if (print_lines == 0)
            {
                std::cout << "All available variables : ";
                for (const auto &var : vars)
                {
                    std::cout << var.first << ", ";
                }
                std::cout << std::endl;
            }
            ASSERT_EQ(vars.size(), 20);
            size_t currentStep = dataManReader.CurrentStep();
            adios2::Variable<char> bpChars =
                dataManIO.InquireVariable<char>("bpChars2");
            adios2::Variable<unsigned char> bpUChars =
                dataManIO.InquireVariable<unsigned char>("bpUChars2");
            adios2::Variable<short> bpShorts =
                dataManIO.InquireVariable<short>("bpShorts2");
            adios2::Variable<unsigned short> bpUShorts =
                dataManIO.InquireVariable<unsigned short>("bpUShorts2");
            adios2::Variable<int> bpInts =
                dataManIO.InquireVariable<int>("bpInts2");
            adios2::Variable<unsigned int> bpUInts =
                dataManIO.InquireVariable<unsigned int>("bpUInts");
            adios2::Variable<float> bpFloats =
                dataManIO.InquireVariable<float>("bpFloats");
            adios2::Variable<double> bpDoubles =
                dataManIO.InquireVariable<double>("bpDoubles");
            adios2::Variable<std::complex<float>> bpComplexes =
                dataManIO.InquireVariable<std::complex<float>>("bpComplexes");
            adios2::Variable<std::complex<double>> bpDComplexes =
                dataManIO.InquireVariable<std::complex<double>>("bpDComplexes");
            auto charsBlocksInfo = dataManReader.AllStepsBlocksInfo(bpChars);

            dataManReader.Get(bpChars, myChars.data(), adios2::Mode::Sync);
            dataManReader.Get(bpUChars, myUChars.data(), adios2::Mode::Sync);
            dataManReader.Get(bpShorts, myShorts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpUShorts, myUShorts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpInts, myInts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpUInts, myUInts.data(), adios2::Mode::Sync);
            dataManReader.Get(bpFloats, myFloats.data(), adios2::Mode::Sync);
            dataManReader.Get(bpDoubles, myDoubles.data(), adios2::Mode::Sync);
            dataManReader.Get(bpComplexes, myComplexes.data(),
                              adios2::Mode::Sync);
            dataManReader.Get(bpDComplexes, myDComplexes.data(),
                              adios2::Mode::Sync);
            VerifyData(myChars.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myUChars.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myShorts.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myUShorts.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myInts.data(), currentStep, Dims(shape.size(), 0), shape,
                       shape);
            VerifyData(myUInts.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myFloats.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myDoubles.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myComplexes.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            VerifyData(myDComplexes.data(), currentStep, Dims(shape.size(), 0),
                       shape, shape);
            dataManReader.EndStep();
        }
        else if (status == adios2::StepStatus::EndOfStream)
        {
            std::cout << "[Rank " + std::to_string(mpiRank) +
                             "] SscTest reader end of stream!"
                      << std::endl;
            break;
        }
    }
    dataManReader.Close();
    print_lines = 0;
}

TEST_F(SscEngineTest, TestSscMultiApp)
{
    std::string filename = "TestSscMultiApp";
    adios2::Params engineParams = {};

    int worldRank, worldSize;
    Dims start, count, shape;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
    if (worldSize < 8)
    {
        return;
    }
    if (worldRank == 0 or worldRank == 1)
    {
        mpiGroup = 0;
    }
    else if (worldRank == 2 or worldRank == 3)
    {
        mpiGroup = 1;
    }
    else if (worldRank == 4 or worldRank == 5)
    {
        mpiGroup = 2;
    }
    else if (worldRank == 6 or worldRank == 7)
    {
        mpiGroup = 3;
    }

    MPI_Comm_split(MPI_COMM_WORLD, mpiGroup, worldRank, &mpiComm);

    MPI_Comm_rank(mpiComm, &mpiRank);
    MPI_Comm_size(mpiComm, &mpiSize);

    size_t steps = 20;

    if (mpiGroup == 0)
    {
        shape = {2, 10};
        start = {(size_t)mpiRank, 0};
        count = {1, 10};
        Writer1(shape, start, count, steps, engineParams, filename);
    }

    if (mpiGroup == 1)
    {
        shape = {2, 10};
        start = {0, 0};
        count = shape;
        Reader1(shape, start, shape, steps, engineParams, filename);
    }

    if (mpiGroup == 2)
    {
        shape = {2, 10};
        start = {(size_t)mpiRank, 0};
        count = {1, 10};
        Writer2(shape, start, count, steps, engineParams, filename);
    }

    if (mpiGroup == 3)
    {
        shape = {2, 10};
        start = {0, 0};
        count = shape;
        Reader2(shape, start, shape, steps, engineParams, filename);
    }

    MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int worldRank, worldSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);
    MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    MPI_Finalize();
    return result;
}