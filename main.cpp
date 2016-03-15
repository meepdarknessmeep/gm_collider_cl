#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include <CL/cl.h>
#include <iostream>
#include <string.h>
#include <string>
#include <thread>
#include "json.hpp"

using json = nlohmann::json;

char *fread(char *file, size_t *outlen)
{

	FILE *f = fopen(file, "rb");
	if (!f)
		return 0;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	char *out = new char[len + 1];
	fseek(f, 0, SEEK_SET);
	fread(out, 1, len, f);
	*outlen = len;
	out[len] = 0;
	return out;
}
uint32_t crc32_bitwise(const void* data, size_t length)
{
	uint32_t crc = ~0; // same as previousCrc32 ^ 0xFFFFFFFF
	const uint8_t* current = (const uint8_t*)data;

	while (length-- != 0)
	{
		crc ^= *current++;

		for (int j = 0; j < 8; j++)
			crc = (crc >> 1) ^ (-int32_t(crc & 1) & 0xedb88320);
	}
	return crc;
}


int main(int argc, char *argv[])
{
	uint32_t testfor;
	std::string SteamID("");
	json j;
	json outjson = json::object();
	bool command_line = false;
	unsigned char complete_fully = 0;
	bool interactive = false;
	bool jsonfile = false;
	if (argc == 3)
	{
		if (!strcmp(argv[1], "-sid"))
		{
			command_line = true;
			SteamID = argv[2];
			SteamID = "gm_" + SteamID;
			testfor = crc32_bitwise(SteamID.c_str(), SteamID.length());
		}
		else if (!strcmp(argv[1], "-uid"))
		{

			sscanf(argv[2], "%u", &testfor);
			testfor = ~testfor;
			command_line = true;
			complete_fully = 1;

		} 

	}
	else if (argc == 2 && !strcmp(argv[1], "-interactive"))
	{
		interactive = true;
		command_line = true;
		complete_fully = 1;
	}
	else if (argc == 4 && !strcmp(argv[1], "-json"))
	{

		jsonfile = true;
		command_line = true;

		size_t jsonlen;
		char *jsontext = fread(argv[2], &jsonlen);
		j = json::parse(jsontext);

		delete[] jsontext;
		complete_fully = 1;

	}

	cl_uint numPlatforms;	//the NO. of platforms
	cl_platform_id platform = NULL;	//the chosen platform
	cl_int status = clGetPlatformIDs(0, NULL, &numPlatforms);
	if (status != CL_SUCCESS)
	{
		printf("Error: getting Platform IDs (%d)", status);
		return 0;
	}

	/*For clarity, choose the first available platform. */
	if (numPlatforms > 0)
	{
		cl_platform_id* platforms = (cl_platform_id*)malloc(numPlatforms* sizeof(cl_platform_id));
		clGetPlatformIDs(numPlatforms, platforms, NULL);
		platform = platforms[0];
		free(platforms);
	}

	/*Step 2:Query the platform and choose the first GPU device if has one.Otherwise use the CPU as device.*/
	cl_uint				numDevices = 0;
	cl_device_id        *devices;
	clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, NULL, &numDevices);
	if (numDevices == 0)	//no GPU available.
	{
		printf("Error: No GPU device available. TODO: Allow CPU\n");
		return 0;
	}
	else
	{
		devices = (cl_device_id*)malloc(numDevices * sizeof(cl_device_id));
		clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, numDevices, devices, NULL);
	}

	cl_context context = clCreateContext(NULL, 1, devices, NULL, NULL, NULL);

	const size_t count = 0xFFFFFFFF;

	size_t ARRAY_LEN = count / 8 + 1;
	if (!command_line)
		printf("Creating some room...\n");

	cl_mem crcOutput = clCreateBuffer(context, CL_MEM_WRITE_ONLY, ARRAY_LEN + 8, NULL, NULL);
	cl_mem crcOutput2 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, ARRAY_LEN + 8, NULL, NULL);
	cl_mem crcCount1 = clCreateBuffer(context, CL_MEM_READ_WRITE, 8, NULL, NULL);
	cl_mem crcCount2 = clCreateBuffer(context, CL_MEM_READ_WRITE, 8, NULL, NULL);


	
	size_t codelen;
	const char *source = fread("opencl_crc.cl", &codelen);

	if (!source)
	{

		printf("Error: Couldn't find opencl file, 'opencl_crc.cl'. Try replacing it with the correct version.\n");
		return 0;

	}
	size_t sourceSize[] = { codelen };

	if (!command_line)
	{
		printf("Type the SteamID you want to search for collisions with.\nSteamID: ");

		while (SteamID.length() == 0)
			std::getline(std::cin, SteamID);
		SteamID = "gm_" + SteamID;
		testfor = crc32_bitwise(SteamID.c_str(), SteamID.length());
	}

	size_t *crc_array = new size_t[j.size()];
	for (size_t i = 0; i < j.size(); i++)
		crc_array[i] = j[i].get<size_t>();
	size_t crc_array_size = jsonfile ? j.size() : 1;

	std::thread *threads = new std::thread[crc_array_size];


	cl_program program = clCreateProgramWithSource(context, 1, &source, sourceSize, NULL);

	status = clBuildProgram(program, 1, devices, NULL, NULL, NULL);

	if (status != CL_SUCCESS)
	{
		// check build error and build status first
		clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_STATUS,
			sizeof(cl_build_status), &status, NULL);

		size_t logSize;
		// check build log
		clGetProgramBuildInfo(program, devices[0],
			CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
		char *programLog = (char*)calloc(logSize + 1, sizeof(char));
		clGetProgramBuildInfo(program, devices[0],
			CL_PROGRAM_BUILD_LOG, logSize + 1, programLog, NULL);
		printf("Error: Build failed; error=%d, status=%d, programLog:nn%s",
			status, status, programLog);
		free(programLog);
		return 0;
	}
	cl_kernel crc = clCreateKernel(program, "crc", NULL);
	cl_kernel crc2 = clCreateKernel(program, "crc2", NULL);
	cl_kernel mklist = clCreateKernel(program, "mklist", NULL);
	cl_kernel mklist2 = clCreateKernel(program, "mklist", NULL);
	cl_kernel zero1 = clCreateKernel(program, "zero", NULL);
	cl_kernel zero2 = clCreateKernel(program, "zero", NULL);

	cl_command_queue commandQueue = clCreateCommandQueue(context, devices[0], 0, NULL);


	size_t jsoni = jsonfile ? 0 : 1;
	while (1)
	{
		if (interactive)
		{
			printf(">\n");

			std::string str;
			std::getline(std::cin, str);

			if (str == "end")
				break;

			sscanf(str.c_str(), "%u", &testfor);
			testfor = ~testfor;
		}
		else if (jsonfile)
		{
			if (crc_array_size <= jsoni)
				break;
			printf("%u left\n", j.size() - jsoni);

			testfor = ~crc_array[jsoni++];
		}



		unsigned int FindCount = 0, FindCount2 = 0;

		cl_mem crcTestFor = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 4, &testfor, NULL);
		cl_mem crcCompleteFully = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 1, &complete_fully, NULL);
		cl_mem crcFindCount = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4, &FindCount, NULL);
		cl_mem crcFindCount2 = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4, &FindCount, NULL);

		clSetKernelArg(zero1, 0, sizeof(cl_mem), (void *)&crcOutput);
		clSetKernelArg(zero2, 0, sizeof(cl_mem), (void *)&crcOutput2);


		size_t array_len_work[1] = { ARRAY_LEN / 8 + 1 };
		cl_event ZeroEvents[2];
		clEnqueueNDRangeKernel(commandQueue, zero1, 1, NULL, array_len_work, NULL, 0, NULL, &ZeroEvents[0]);
		clEnqueueNDRangeKernel(commandQueue, zero2, 1, NULL, array_len_work, NULL, 0, NULL, &ZeroEvents[1]);


		clSetKernelArg(crc, 0, sizeof(cl_mem), (void *)&crcOutput);
		clSetKernelArg(crc2, 0, sizeof(cl_mem), (void *)&crcOutput2);
		clSetKernelArg(crc, 1, sizeof(cl_mem), (void *)&crcTestFor);
		clSetKernelArg(crc2, 1, sizeof(cl_mem), (void *)&crcTestFor);
		clSetKernelArg(crc, 2, sizeof(cl_mem), (void *)&crcCompleteFully);
		clSetKernelArg(crc2, 2, sizeof(cl_mem), (void *)&crcCompleteFully);
		clSetKernelArg(crc, 3, sizeof(cl_mem), (void *)&crcFindCount);
		clSetKernelArg(crc2, 3, sizeof(cl_mem), (void *)&crcFindCount2);

		size_t global_work_size[1] = { count };
		if (!command_line)
			printf("Running the collision finder...\n");

		 clEnqueueNDRangeKernel(commandQueue, crc, 1, NULL, global_work_size, NULL, 1, &ZeroEvents[0], NULL);
		 clEnqueueNDRangeKernel(commandQueue, crc2, 1, NULL, global_work_size, NULL, 1, &ZeroEvents[1], NULL);


		clFinish(commandQueue);
		// setup finders arrays
		if (!command_line)
			printf("Sorting...\n");

		 clEnqueueReadBuffer(commandQueue, crcFindCount, CL_TRUE, 0, 4, &FindCount, 0, NULL, NULL);
		cl_mem crcFindArray1 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 4 * (FindCount + 1), NULL, NULL);


		size_t crc_null_work[1] = { 1 };
		clSetKernelArg(zero1, 0, sizeof(cl_mem), (void *)&crcCount1);
		clSetKernelArg(zero2, 0, sizeof(cl_mem), (void *)&crcCount2);

		clEnqueueNDRangeKernel(commandQueue, zero1, 1, NULL, crc_null_work, NULL, 0, NULL, &ZeroEvents[0]);
		clEnqueueNDRangeKernel(commandQueue, zero2, 1, NULL, crc_null_work, NULL, 0, NULL, &ZeroEvents[1]);

		clSetKernelArg(mklist, 0, sizeof(cl_mem), (void *)&crcCount1);
		clSetKernelArg(mklist, 1, sizeof(cl_mem), (void *)&crcOutput);
		clSetKernelArg(mklist, 2, sizeof(cl_mem), (void *)&crcFindArray1);
		size_t ff1[1] = { ARRAY_LEN };
		clEnqueueNDRangeKernel(commandQueue, mklist, 1, NULL, ff1, NULL, 1, &ZeroEvents[0], NULL);

		// second finder for STEAM_0:1
		 clEnqueueReadBuffer(commandQueue, crcFindCount2, CL_TRUE, 0, 4, &FindCount2, 0, NULL, NULL);
		cl_mem crcFindArray2 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 4 * (FindCount2 + 1), NULL, NULL);

		clSetKernelArg(mklist2, 0, sizeof(cl_mem), (void *)&crcCount2);
		clSetKernelArg(mklist2, 1, sizeof(cl_mem), (void *)&crcOutput2);
		clSetKernelArg(mklist2, 2, sizeof(cl_mem), (void *)&crcFindArray2);
		clEnqueueNDRangeKernel(commandQueue, mklist2, 1, NULL, ff1, NULL, 1, &ZeroEvents[1], NULL);

		clFinish(commandQueue);

		size_t *crcs = new size_t[FindCount2 + FindCount + 1];

		if (FindCount > 0)
			 clEnqueueReadBuffer(commandQueue, crcFindArray1, CL_TRUE, 0, 4 * FindCount, crcs, 0, NULL, NULL);
		if (FindCount2 > 0)
			 clEnqueueReadBuffer(commandQueue, crcFindArray2, CL_TRUE, 0, 4 * FindCount2, &crcs[FindCount], 0, NULL, NULL);

		threads[jsoni - 1] = std::thread([&]()
		{
			static char index[128];
			sprintf(index, "%u", ~testfor);
			if (jsonfile)
				outjson[index] = json::array();
			for (size_t z = 0; z < FindCount + FindCount2; z++)
			{
				static char outputstr[128];
				sprintf(outputstr, "STEAM_0:%u:%u", z >= FindCount, crcs[z]);
				printf("%s%s\n", command_line ? "" : "Found: ", outputstr);
				if (jsonfile)
					outjson[index].push_back(outputstr);
			}

			delete[] crcs;

		});

		clReleaseMemObject(crcTestFor);		//Release mem object.
		clReleaseMemObject(crcFindCount);		//Release mem object.
		clReleaseMemObject(crcFindCount2);		//Release mem object.
		clReleaseMemObject(crcFindArray2);		//Release mem object.
		clReleaseMemObject(crcFindArray1);		//Release mem object.
		clReleaseMemObject(crcCompleteFully);		//Release mem object.

		if (!jsonfile && !interactive)
			break;
		if (interactive)
			threads[0].join();
	}
	clReleaseContext(context);				//Release context.
	clReleaseMemObject(crcOutput);		//Release mem object.
	clReleaseMemObject(crcOutput2);		//Release mem object.
	clReleaseMemObject(crcCount2);		//Release mem object.
	clReleaseMemObject(crcCount1);		//Release mem object.
	clReleaseKernel(crc);				//Release kernel.
	clReleaseKernel(crc2);				//Release kernel.
	clReleaseKernel(mklist);				//Release kernel.
	clReleaseKernel(mklist2);				//Release kernel.
	clReleaseKernel(zero1);				//Release kernel.
	clReleaseKernel(zero2);				//Release kernel.
	clReleaseProgram(program);				//Release the program object.
	clReleaseCommandQueue(commandQueue);	//Release  Command queue.

	for (int i = 0; i < crc_array_size; i++)
	{
		threads[i].join();
	}

	delete[] threads;
	if (!command_line)
		printf("BYE!");

	delete[] source;

	if (jsonfile)
	{
		FILE *f = fopen(argv[3], "wb");
		std::string dump = outjson.dump();
		fwrite(dump.c_str(), 1, dump.length(), f);
		fclose(f);
	}

	return 0;
	

}