#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include "CL/cl.h"
#include <iostream>
#include <string.h>
#include <string>
#include <atomic>
#include <thread>
#include "json.hpp"


#define count 200000000
const cl_uint threads_at_once = 1;

using json = nlohmann::json;

bool fcanaccess(const char *file)
{

	FILE *f = fopen(file, "wb");
	if (f)
		fclose(f);

	return f != 0;
}

char *fread(const char *file, size_t *outlen)
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
uint32_t crc32_bitwise(const void* data, cl_uint length)
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

cl_int check(cl_int status)
{
	if (status != CL_SUCCESS)
		printf("Error: status == %d\n", status);

	return status;
}

void clGetDeviceName(std::string &str, cl_device_id id)
{
	size_t info_size;
	clGetDeviceInfo(id, CL_DEVICE_NAME, 0, NULL, &info_size);
	char *name = new char[info_size+1];
	clGetDeviceInfo(id, CL_DEVICE_NAME, info_size + 1, name, NULL);
	name[info_size] = 0;

	info_size;
	clGetDeviceInfo(id, CL_DEVICE_VENDOR, 0, NULL, &info_size);
	char *vendor = new char[info_size+1];
	clGetDeviceInfo(id, CL_DEVICE_VENDOR, info_size + 1, vendor, NULL);
	vendor[info_size] = 0;

	str = std::string(vendor) + " " + name;

	delete[] name;
	delete[] vendor;
	
}

void Thread(cl_mem &mem, cl_command_queue &commandQueue, cl_kernel *kernels, bool command_line, cl_program &program, json &outjson, cl_context &context, cl_uint jsoni, cl_uint testfor, bool jsonfile)
{
	const cl_uint max_finds = 100;

	const cl_uint ARRAY_LEN = max_finds * 4;
	cl_mem crcOutput = clCreateBuffer(context, CL_MEM_WRITE_ONLY, ARRAY_LEN, NULL, NULL);
	cl_mem crcOutput2 = clCreateBuffer(context, CL_MEM_WRITE_ONLY, ARRAY_LEN, NULL, NULL);
	cl_mem crcCount1 = clCreateBuffer(context, CL_MEM_READ_WRITE, 4, NULL, NULL);
	cl_mem crcCount2 = clCreateBuffer(context, CL_MEM_READ_WRITE, 4, NULL, NULL);

	
	unsigned int FindCount = 0, FindCount2 = 0;
	cl_uint testfor2 = testfor;

	cl_mem crcTestFor = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 4, &testfor2, NULL);
	cl_mem crcFindCount = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4, &FindCount, NULL);
	cl_mem crcFindCount2 = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4, &FindCount, NULL);


	check(clSetKernelArg(kernels[0], 0, sizeof(cl_mem), (void *)&crcOutput));
	check(clSetKernelArg(kernels[1], 0, sizeof(cl_mem), (void *)&crcOutput2));
	check(clSetKernelArg(kernels[0], 1, sizeof(cl_mem), (void *)&crcTestFor));
	check(clSetKernelArg(kernels[1], 1, sizeof(cl_mem), (void *)&crcTestFor));
	check(clSetKernelArg(kernels[0], 2, sizeof(cl_mem), (void *)&mem));
	check(clSetKernelArg(kernels[1], 2, sizeof(cl_mem), (void *)&mem));
	check(clSetKernelArg(kernels[0], 3, sizeof(cl_mem), (void *)&crcFindCount));
	check(clSetKernelArg(kernels[1], 3, sizeof(cl_mem), (void *)&crcFindCount2));

	size_t global_work_size[1] = { count };

	cl_event Events[4];
	check(clEnqueueNDRangeKernel(commandQueue, kernels[0], 1, NULL, global_work_size, NULL, 0, NULL, &Events[0]));
	check(clEnqueueNDRangeKernel(commandQueue, kernels[1], 1, NULL, global_work_size, NULL, 0, NULL, &Events[1]));

	check(clEnqueueReadBuffer(commandQueue, crcFindCount2, CL_TRUE, 0, 4, &FindCount2, 1, &Events[0], &Events[2]));
	check(clEnqueueReadBuffer(commandQueue, crcFindCount, CL_TRUE, 0, 4, &FindCount, 1, &Events[1], &Events[3]));

	cl_uint output1[max_finds], output2[max_finds];
	if (FindCount > 0)
		check(clEnqueueReadBuffer(commandQueue, crcOutput, CL_TRUE, 0, 4 * FindCount, output1, 1, &Events[2], NULL));
	if (FindCount2 > 0)
		check(clEnqueueReadBuffer(commandQueue, crcOutput2, CL_TRUE, 0, 4 * FindCount2, output2, 1, &Events[3], NULL));

	char index[128];
	char outputstr[128];
	sprintf(index, "%u", ~testfor);
	if (jsonfile)
		outjson[index] = json::array();

	for (cl_uint z = 0; z < FindCount; z++)
	{
		sprintf(outputstr, "STEAM_0:0:%u", output1[z]);
		printf("%u %s\n", jsoni, outputstr);
		if (jsonfile)
			outjson[index].push_back(outputstr);
	}
	for (cl_uint z = 0; z < FindCount2; z++)
	{
		sprintf(outputstr, "STEAM_0:1:%u", output2[z]);
		printf("%u %s\n", jsoni, outputstr);
		if (jsonfile)
			outjson[index].push_back(outputstr);
	}
	check(clReleaseMemObject(crcCount1));
	check(clReleaseMemObject(crcCount2));
	check(clReleaseMemObject(crcOutput));
	check(clReleaseMemObject(crcOutput2));
	check(clReleaseMemObject(crcTestFor));		//Release mem object.
	check(clReleaseMemObject(crcFindCount));		//Release mem object.
	check(clReleaseMemObject(crcFindCount2));		//Release mem object.
	check(clReleaseEvent(Events[0]));
	check(clReleaseEvent(Events[1]));
	check(clReleaseEvent(Events[2]));
	check(clReleaseEvent(Events[3]));

	clFinish(commandQueue);


};

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

	cl_uint numPlatforms;
	cl_int status = check(clGetPlatformIDs(0, NULL, &numPlatforms));
	if (status != CL_SUCCESS)
	{
		printf("Error: getting Platform IDs (%d)\n", status);
		if (status == -1001) 
			printf("Your error is from ICD registration. If you are on linux, set your environment variable for OPENCL_VENDOR_PATH to /etc/OpenCL/vendors (default path). Make sure you have your drivers installed.\n");
		return 0;
	}

	if (numPlatforms == 0) 
	{
		printf("Error: numPlatforms == 0\n");
		return 0;
	}
	cl_platform_id* platforms = new cl_platform_id[numPlatforms];
	check(clGetPlatformIDs(numPlatforms, platforms, NULL));
	printf("%u platforms.\n", numPlatforms);


	cl_uint      numDevices = 0;
	cl_uint      *numDevicesPlatform = new cl_uint[numPlatforms];
	cl_device_id *devices;
	for (cl_uint i = 0; i < numPlatforms; i++)
	{
		check(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 0, NULL, &numDevicesPlatform[i]));
		numDevices += numDevicesPlatform[i];
	}
	devices = new cl_device_id[numDevices];
	
	cl_uint devices_total = 0;

	for (cl_uint i = 0; i < numPlatforms; i++)
	{
		check(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, numDevicesPlatform[i], &devices[devices_total], NULL));
		devices_total += numDevicesPlatform[i];
	}

	delete[] numDevicesPlatform;


	size_t codelen;
	const char *source = fread("opencl_crc.cl", &codelen);

	if (!source)
	{

		printf("Error: Couldn't find opencl file, 'opencl_crc.cl'. Try replacing it with the correct version.\n");
		return 0;

	}

	if (!command_line)
	{
		printf("Type the SteamID you want to search for collisions with.\nSteamID: ");

		while (SteamID.length() == 0)
			std::getline(std::cin, SteamID);
		SteamID = "gm_" + SteamID;
		testfor = crc32_bitwise(SteamID.c_str(), SteamID.length());
	}

	cl_command_queue *commandQueues = new cl_command_queue[numDevices];
	auto kernels = new cl_kernel[numDevices][2];
	cl_program *programs = new cl_program[numDevices];
	cl_context *contexts = new cl_context[numDevices];
	cl_mem *mems = new cl_mem[numDevices];
	for (cl_uint i = 0; i < numDevices; i++)
	{
		std::string name;
		clGetDeviceName(name, devices[i]);
		printf("Using %s\n", name.c_str());
		contexts[i] = clCreateContext(NULL, 1, &devices[i], NULL, NULL, NULL);
		programs[i] = clCreateProgramWithSource(contexts[i], 1, &source, &codelen, NULL);
		status = check(clBuildProgram(programs[i], 1, &devices[i], NULL, NULL, NULL));
		if (status != CL_SUCCESS)
		{
			// check build error and build status first
			clGetProgramBuildInfo(programs[i], devices[0], CL_PROGRAM_BUILD_STATUS,
				sizeof(cl_build_status), &status, NULL);

			size_t logSize;
			// check build log
			clGetProgramBuildInfo(programs[i], devices[0],
				CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
			char *programLog = (char*)calloc(logSize + 1, sizeof(char));
			clGetProgramBuildInfo(programs[i], devices[0],
				CL_PROGRAM_BUILD_LOG, logSize + 1, programLog, NULL);
			printf("Error: Build failed; error=%d, status=%d, programLog:nn%s",
				status, status, programLog);
			free(programLog);
			return 0;
		}


		kernels[i][0] = clCreateKernel(programs[i], "crc", NULL);
		kernels[i][1] = clCreateKernel(programs[i], "crc2", NULL);
		mems[i] = clCreateBuffer(contexts[i], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 1, &complete_fully, NULL);


		commandQueues[i] = clCreateCommandQueue(contexts[i], devices[i], 0, NULL);
	}


	if (jsonfile) // json mode
	{
		cl_uint *crc_array = new cl_uint[j.size()];
		for (cl_uint i = 0; i < j.size(); i++)
			crc_array[i] = ~j[i].get<cl_uint>();
		cl_uint crc_array_size = j.size();

		std::thread *threads = new std::thread[numDevices];
		std::atomic<cl_uint> jsoni(0);

		for (cl_uint i = 0; i < numDevices; i++)
		{
			threads[i] = std::thread([i, &crc_array_size, &mems, &commandQueues, &kernels, command_line, &programs, &outjson, &jsoni, &contexts, jsonfile, &crc_array]()
			{
				cl_uint currentjsoni;
				while((currentjsoni = jsoni.fetch_add(1)) < crc_array_size)
					Thread(mems[i], commandQueues[i], kernels[i], command_line, programs[i], outjson, contexts[i], currentjsoni, crc_array[currentjsoni], jsonfile);

			});
		}
		for (cl_uint i = 0; i < numDevices; i++)
			threads[i].join();
		delete[] crc_array;
	}
	else
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


			Thread(mems[0], commandQueues[0], kernels[0], command_line, programs[0], outjson, contexts[0], 0, testfor, jsonfile);


			if (!interactive)
				break;
		}

	for (cl_uint i = 0; i < numDevices; i++)
	{
		check(clReleaseKernel(kernels[i][0]));
		check(clReleaseKernel(kernels[i][1]));
		check(clReleaseMemObject(mems[i]));
		check(clReleaseCommandQueue(commandQueues[i]));
		check(clReleaseProgram(programs[i]));
		check(clReleaseContext(contexts[i]));
	}

	if (!command_line)
		printf("BYE!\n");

	delete[] source;
	delete[] contexts;
	delete[] programs;
	delete[] kernels;
	delete[] commandQueues;
	delete[] mems;
	delete[] devices;


	if (jsonfile)
	{
		FILE *f = fopen(argv[3], "wb");
		std::string dump = outjson.dump();
		fwrite(dump.c_str(), 1, dump.length(), f);
		fclose(f);
	}

	return 0;
	

}
