#include <gtest/gtest.h>
#include <string>
#include <mpi.h>
#include <stk_io/StkMeshIoBroker.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <fieldNameTestUtils.hpp>
#include <restartTestUtils.hpp>

namespace {

TEST(StkMeshIoBrokerHowTo, restartWithMultistateField)
{
    std::string restartFilename = "output.restart";
    MPI_Comm communicator = MPI_COMM_WORLD;
    const std::string fieldName = "disp";
    const double stateNp1Value = 1.0;
    const double stateNValue = 2.0;
    const double stateNm1Value = 3.0;
    double time = 0.0;
    {
        stk::io::StkMeshIoBroker stkIo(communicator);
	const std::string exodusFileName = "generated:1x1x8";
	size_t index = stkIo.add_mesh_database(exodusFileName, stk::io::READ_MESH);
	stkIo.create_input_mesh(index);
        stk::mesh::MetaData &stkMeshMetaData = stkIo.meta_data();
        stk::mesh::FieldBase *triStateField =
                declareTriStateNodalField(stkMeshMetaData, fieldName);
        stkIo.populate_bulk_data(index);

        putDataOnTriStateField(stkIo.bulk_data(), triStateField,
                stateNp1Value, stateNValue, stateNm1Value);

        size_t fileHandle =
	  stkIo.create_output_mesh(restartFilename, stk::io::WRITE_RESTART);
        stkIo.add_field(fileHandle, *triStateField);

        stkIo.begin_output_step(fileHandle, time);
        stkIo.write_defined_output_fields(fileHandle);
        stkIo.end_output_step(fileHandle);
    }

    //code to test that the field was written correctly
    testMultistateFieldWroteCorrectlyToRestart(restartFilename, time,
            fieldName, stateNp1Value, stateNValue);
    unlink(restartFilename.c_str());
}
}
