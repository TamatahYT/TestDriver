#include <ntifs.h>
#include <ntddk.h>
#include "..\TestDriver\Header.h"

void DriverTestUnload(DRIVER_OBJECT);//unload routine 
NTSTATUS DriverTestCreateClose(PDEVICE_OBJECT, PIRP);
NTSTATUS DriverTestDeviceControl(PDEVICE_OBJECT, PIRP);

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {

	KdPrint(("DriverTest: DriverEntry\n"));
	KdPrint(("DriverTest: RegistryPath(%wZ)\n", RegistryPath));
	
	RTL_OSVERSIONINFOW vi = { sizeof(vi) };
	NTSTATUS status = RtlGetVersion(&vi);
	if (!NT_SUCCESS(status)) {
		KdPrint(("DriverTest: failed to RtlGetVersion 0x(%X)\n",status));
		return status;
	}
	KdPrint(("ProcessPower: Windows version %u.%u.%u", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber));

	DriverObject->MajorFunction[IRP_MJ_CREATE]			= DriverTestCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE]			= DriverTestCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]	= DriverTestDeviceControl;

	UNICODE_STRING devname;
	RtlInitUnicodeString(&devname, L"\\Device\\DriverTest");
	PDEVICE_OBJECT DeviceObject;
	status = IoCreateDevice(DriverObject, 0, &devname, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);

	if (!NT_SUCCESS(status)) {
		KdPrint(("DriverTest: failed to IoCreateDevice 0x(%X)\n", status));
		return status;
	}
	UNICODE_STRING symLink;
	RtlInitUnicodeString(&symLink, L"\\??\\DriverTest");
	status = IoCreateSymbolicLink(&symLink, &devname);

	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(DeviceObject);//should delete the device object if it fail because it will leak it if exit
		KdPrint(("DriverTest: failed to IoCreateSymbolicLink 0x(%X)\n", status));
		return status;
	}

	DriverObject->DriverUnload = (PDRIVER_UNLOAD)DriverTestUnload;
	return STATUS_SUCCESS;
}

NTSTATUS DriverTestCreateClose(PDEVICE_OBJECT, PIRP Irp) {
	Irp->IoStatus.Status = STATUS_SUCCESS;//This mean filling the request with status success, the create close file accepted 
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, 0);//complete the irp with boost piority 0 and if there's asychronization, we can change it
	return STATUS_SUCCESS;
}

NTSTATUS DriverTestDeviceControl(PDEVICE_OBJECT, PIRP Irp) {
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);//getting the stack 
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	auto& dic = stack->Parameters.DeviceIoControl;//the parameteres from the user which connected to the device
	ULONG len = 0;
	switch (dic.IoControlCode) {// for the control code from client connected

	case IOCTL_OPEN_PROCESS:{
		if (dic.Type3InputBuffer == nullptr || Irp->UserBuffer == nullptr) {// checking the input from the user and output from the request is nullptr
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		if (dic.InputBufferLength < sizeof(USERINPUT) || dic.OutputBufferLength < sizeof(USEROUTPUT)) {//checking the size of inout and output
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		USERINPUT* input = (USERINPUT*)dic.Type3InputBuffer;//casting the input from the struct which is process id
		USEROUTPUT* output = (USEROUTPUT*)Irp->UserBuffer;//the handle of the process for our purpse
		OBJECT_ATTRIBUTES attr;
		InitializeObjectAttributes(&attr, nullptr, 0, nullptr, nullptr);
		CLIENT_ID cid = { 0 };
		cid.UniqueProcess = ULongToHandle(input->procID);
		status = ZwOpenProcess(&output->procHandle, PROCESS_ALL_ACCESS, &attr, &cid);
		if (NT_SUCCESS(status)) {
			len = sizeof(output);
		}
		break;
	}
	case IOCTL_BOOSTER_THREAD: {
		if (dic.InputBufferLength < sizeof(ThreadData)) {//check if the parameter length smaller than struct
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		auto data = (ThreadData*)Irp->AssociatedIrp.SystemBuffer;//because METHOD_BUFFERED means the buffer in the system
		if (data == nullptr) {//check if the parameter not null
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		if (data->priority < 1 || data->priority > 31) {//check if the priority value is between 1-31
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		PETHREAD THREAD;
		status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadID), &THREAD);
		if (!NT_SUCCESS(status))
			break;

		KeSetPriorityThread((PKTHREAD)THREAD, data->priority);
		ObDereferenceObject(THREAD);
		break;

	}
	
	}

	Irp->IoStatus.Status = status;//This mean filling the request with status success, the create close file accepted 
	Irp->IoStatus.Information = len;
	IoCompleteRequest(Irp, 0);//complete the irp with boost piority 0 and if there's asychronization, we can change it
	return status;
}

void DriverTestUnload(DRIVER_OBJECT DriverObject) {
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\DriverTest");
	IoDeleteSymbolicLink(&symLink);
	KdPrint(("DriverTest: DriverUnload\n"));
	IoDeleteDevice(DriverObject.DeviceObject);

}