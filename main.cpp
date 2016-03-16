#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include "CL/cl.h"
#include <iostream>
#include <string.h>
#include <string>
#include <mutex>
#include <thread>
#include "json.hpp"


#define count 200000000

using json = nlohmann::json;

bool fcanaccess(const char *file)
{

	FILE *f = fopen(file, "rb");
	if (f)
		fclose(f);

	return f != 0;
}

char *fread(const char *file, size_t *outlen)
{

	FILE *f = fopen(file, "wb");
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
		if (!jsontext)
		{
			printf("Error: File %s not found.\n", argv[2]);
			return 0;
		}
		if (!fcanaccess(argv[3]))
		{
			printf("Error: File %s cannot be opened.\n", argv[3]);
			return 0;
		}
		j = json::parse(jsontext);

		delete[] jsontext;
		complete_fully = 1;

	}

	cl_uint numPlatforms;	//the NO. of platforms
	cl_platform_id platform = NULL;	//the chosen platform
	cl_int status = clGetPlatformIDs(0, NULL, &numPlatforms);
	if (status != CL_SUCCESS)
	{
		printf("Error: getting Platform IDs (%d)\n", status);
		if (status == -1001) 
			printf("Your error is from ICD registration. If you are on linux, set your environment variable for OPENCL_VENDOR_PATH to /etc/OpenCL/vendors (default path). Make sure you have your drivers installed.\n");
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

	const size_t threads_at_once = 2;

	std::thread threads[threads_at_once];


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

	std::mutex m;
	size_t jsoni = 0;
	cl_command_queue commandQueue = clCreateCommandQueue(context, devices[0], 0, NULL);
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

			testfor = ~crc_array[jsoni];
		}


		threads[jsoni++ % threads_at_once] = std::thread([&commandQueue, &crc, &crc2, &m, &command_line, &complete_fully, &program, &outjson, &context, jsoni, testfor, jsonfile]()
		{
			const size_t max_finds = 100;

			const size_t ARRAY_LEN = max_finds * 4;
			cl_mem crcOutput = clCreateBuffer(context, CL_MEM_WRITE_ONLY, ARRAY_LEN, NULL, NULL);
			cl_mem crcOutput2 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, ARRAY_LEN, NULL, NULL);
			cl_mem crcCount1 = clCreateBuffer(context, CL_MEM_READ_WRITE, 8, NULL, NULL);
			cl_mem crcCount2 = clCreateBuffer(context, CL_MEM_READ_WRITE, 8, NULL, NULL);

			
			unsigned int FindCount = 0, FindCount2 = 0;
			size_t testfor2 = testfor;

			cl_mem crcTestFor = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 4, &testfor2, NULL);
			cl_mem crcCompleteFully = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 1, &complete_fully, NULL);
			cl_mem crcFindCount = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4, &FindCount, NULL);
			cl_mem crcFindCount2 = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4, &FindCount, NULL);


			m.lock();
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

			cl_event Events[4];
			clEnqueueNDRangeKernel(commandQueue, crc, 1, NULL, global_work_size, NULL, 0, NULL, &Events[0]);
			clEnqueueNDRangeKernel(commandQueue, crc2, 1, NULL, global_work_size, NULL, 0, NULL, &Events[1]);
			m.unlock();

			clEnqueueReadBuffer(commandQueue, crcFindCount2, CL_TRUE, 0, 4, &FindCount2, 1, &Events[0], &Events[2]);
			clEnqueueReadBuffer(commandQueue, crcFindCount, CL_TRUE, 0, 4, &FindCount, 1, &Events[1], &Events[3]);

			size_t output1[max_finds], output2[max_finds];
			if (FindCount > 0)
				clEnqueueReadBuffer(commandQueue, crcOutput, CL_TRUE, 0, 4 * FindCount, output1, 1, &Events[2], NULL);
			if (FindCount2 > 0)
				clEnqueueReadBuffer(commandQueue, crcOutput2, CL_TRUE, 0, 4 * FindCount2, output2, 1, &Events[3], NULL);

			char index[128];
			char outputstr[128];
			sprintf(index, "%u", ~testfor);
			if (jsonfile)
				outjson[index] = json::array();

			for (size_t z = 0; z < FindCount; z++)
			{
				sprintf(outputstr, "%u STEAM_0:0:%u", jsoni, output1[z]);
				printf("%s%s\n", command_line ? "" : "Found: ", outputstr);
				if (jsonfile)
					outjson[index].push_back(outputstr);
			}
			for (size_t z = 0; z < FindCount2; z++)
			{
				sprintf(outputstr, "%u STEAM_0:1:%u", jsoni, output2[z]);
				printf("%s%s\n", command_line ? "" : "Found: ", outputstr);
				if (jsonfile)
					outjson[index].push_back(outputstr);
			}
			clReleaseEvent(Events[0]);
			clReleaseEvent(Events[1]);
			clReleaseEvent(Events[2]);
			clReleaseEvent(Events[3]);
			clReleaseMemObject(crcTestFor);		//Release mem object.
			clReleaseMemObject(crcFindCount);		//Release mem object.
			clReleaseMemObject(crcFindCount2);		//Release mem object.
			clReleaseMemObject(crcCompleteFully);		//Release mem object.


		});

		if (!jsonfile && !interactive)
			break;
		if (interactive)
			threads[0].join();
		else if (jsoni % threads_at_once == 0)
			for (size_t i = 0; i < threads_at_once; i++)
				threads[i].join();
	}
	
	if (!interactive)
		printf("Waiting for threads...\n");

	if (jsonfile && jsoni % threads_at_once != 0)
		for (size_t i = 0; i < jsoni % threads_at_once; i++)
			threads[i].join();
	else if (!jsonfile)
		threads[0].join();

	clReleaseContext(context);				//Release context.
	clReleaseProgram(program);				//Release the program object.
			clReleaseCommandQueue(commandQueue);	//Release  Command queue.

	if (!command_line)
		printf("BYE!");

	delete[] source;
	delete[] crc_array;

	clReleaseKernel(crc);				//Release kernel.
	clReleaseKernel(crc2);				//Release kernel.
	if (jsonfile)
	{
		FILE *f = fopen(argv[3], "wb");
		std::string dump = outjson.dump();
		fwrite(dump.c_str(), 1, dump.length(), f);
		fclose(f);
	}

	return 0;
	

}
