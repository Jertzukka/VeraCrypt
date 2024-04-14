/*
 Derived from source code of TrueCrypt 7.1a, which is
 Copyright (c) 2008-2012 TrueCrypt Developers Association and which is governed
 by the TrueCrypt License 3.0.

 Modifications and additions to the original source code (contained in this file)
 and all other portions of this file are Copyright (c) 2013-2017 IDRIX
 and are governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages.
*/

#include <iostream>
#include "Common/Tcdefs.h"
#include "VolumeInfo.h"
#include "Platform/SerializerFactory.h"

namespace VeraCrypt
{
	void VolumeInfo::Deserialize (shared_ptr <Stream> stream)
	{
		Serializer sr (stream);

		sr.Deserialize ("ProgramVersion", ProgramVersion);
		AuxMountPoint = sr.DeserializeWString ("AuxMountPoint");
		sr.Deserialize ("EncryptionAlgorithmBlockSize", EncryptionAlgorithmBlockSize);
		sr.Deserialize ("EncryptionAlgorithmKeySize", EncryptionAlgorithmKeySize);
		sr.Deserialize ("EncryptionAlgorithmMinBlockSize", EncryptionAlgorithmMinBlockSize);
		EncryptionAlgorithmName = sr.DeserializeWString ("EncryptionAlgorithmName");
		EncryptionModeName = sr.DeserializeWString ("EncryptionModeName");
		sr.Deserialize ("HeaderCreationTime", HeaderCreationTime);
		sr.Deserialize ("HiddenVolumeProtectionTriggered", HiddenVolumeProtectionTriggered);
		LoopDevice = sr.DeserializeWString ("LoopDevice");

		if (ProgramVersion >= 0x10b)
			sr.Deserialize ("MinRequiredProgramVersion", MinRequiredProgramVersion);

		MountPoint = sr.DeserializeWString ("MountPoint");
		Path = sr.DeserializeWString ("Path");
		sr.Deserialize ("Pkcs5IterationCount", Pkcs5IterationCount);
		Pkcs5PrfName = sr.DeserializeWString ("Pkcs5PrfName");
		Protection = static_cast <VolumeProtection::Enum> (sr.DeserializeInt32 ("Protection"));
		sr.Deserialize ("SerialInstanceNumber", SerialInstanceNumber);
		sr.Deserialize ("Size", Size);
		sr.Deserialize ("SlotNumber", SlotNumber);

		if (ProgramVersion >= 0x10b)
			sr.Deserialize ("SystemEncryption", SystemEncryption);

		if (ProgramVersion >= 0x10b)
			sr.Deserialize ("TopWriteOffset", TopWriteOffset);

		sr.Deserialize ("TotalDataRead", TotalDataRead);
		sr.Deserialize ("TotalDataWritten", TotalDataWritten);
		Type = static_cast <VolumeType::Enum> (sr.DeserializeInt32 ("Type"));
		VirtualDevice = sr.DeserializeWString ("VirtualDevice");
		sr.Deserialize ("VolumeCreationTime", VolumeCreationTime);
		sr.Deserialize ("Pim", Pim);
	}

	bool VolumeInfo::FirstVolumeMountedAfterSecond (shared_ptr <VolumeInfo> first, shared_ptr <VolumeInfo> second)
	{
		return first->SerialInstanceNumber > second->SerialInstanceNumber;
	}

	void VolumeInfo::Serialize (shared_ptr <Stream> stream) const
	{
		Serializable::Serialize (stream);
		Serializer sr (stream);

		const uint32 version = VERSION_NUM;
		sr.Serialize ("ProgramVersion", version);
		sr.Serialize ("AuxMountPoint", wstring (AuxMountPoint));
		sr.Serialize ("EncryptionAlgorithmBlockSize", EncryptionAlgorithmBlockSize);
		sr.Serialize ("EncryptionAlgorithmKeySize", EncryptionAlgorithmKeySize);
		sr.Serialize ("EncryptionAlgorithmMinBlockSize", EncryptionAlgorithmMinBlockSize);
		sr.Serialize ("EncryptionAlgorithmName", EncryptionAlgorithmName);
		sr.Serialize ("EncryptionModeName", EncryptionModeName);
		sr.Serialize ("HeaderCreationTime", HeaderCreationTime);
		sr.Serialize ("HiddenVolumeProtectionTriggered", HiddenVolumeProtectionTriggered);
		sr.Serialize ("LoopDevice", wstring (LoopDevice));
		sr.Serialize ("MinRequiredProgramVersion", MinRequiredProgramVersion);
		sr.Serialize ("MountPoint", wstring (MountPoint));
		sr.Serialize ("Path", wstring (Path));
		sr.Serialize ("Pkcs5IterationCount", Pkcs5IterationCount);
		sr.Serialize ("Pkcs5PrfName", Pkcs5PrfName);
		sr.Serialize ("Protection", static_cast <uint32> (Protection));
		sr.Serialize ("SerialInstanceNumber", SerialInstanceNumber);
		sr.Serialize ("Size", Size);
		sr.Serialize ("SlotNumber", SlotNumber);
		sr.Serialize ("SystemEncryption", SystemEncryption);
		sr.Serialize ("TopWriteOffset", TopWriteOffset);
		sr.Serialize ("TotalDataRead", TotalDataRead);
		sr.Serialize ("TotalDataWritten", TotalDataWritten);
		sr.Serialize ("Type", static_cast <uint32> (Type));
		sr.Serialize ("VirtualDevice", wstring (VirtualDevice));
		sr.Serialize ("VolumeCreationTime", VolumeCreationTime);
		sr.Serialize ("Pim", Pim);
	}

	void VolumeInfo::Set (const Volume &volume)
	{
		EncryptionAlgorithmBlockSize = static_cast <uint32> (volume.GetEncryptionAlgorithm()->GetMaxBlockSize());
		EncryptionAlgorithmKeySize = static_cast <uint32> (volume.GetEncryptionAlgorithm()->GetKeySize());
		EncryptionAlgorithmMinBlockSize = static_cast <uint32> (volume.GetEncryptionAlgorithm()->GetMinBlockSize());
		EncryptionAlgorithmName = volume.GetEncryptionAlgorithm()->GetName();
		EncryptionModeName = volume.GetEncryptionMode()->GetName();
		HeaderCreationTime = volume.GetHeaderCreationTime();
		VolumeCreationTime = volume.GetVolumeCreationTime();
		HiddenVolumeProtectionTriggered = volume.IsHiddenVolumeProtectionTriggered();
		MinRequiredProgramVersion = volume.GetHeader()->GetRequiredMinProgramVersion();
		Path = volume.GetPath();
		Pkcs5IterationCount = volume.GetPkcs5Kdf()->GetIterationCount(volume.GetPim ());
		Pkcs5PrfName = volume.GetPkcs5Kdf()->GetName();
		Protection = volume.GetProtectionType();
		Size = volume.GetSize();
		SystemEncryption = volume.IsInSystemEncryptionScope();
		Type = volume.GetType();
		TopWriteOffset = volume.GetTopWriteOffset();
		TotalDataRead = volume.GetTotalDataRead();
		TotalDataWritten = volume.GetTotalDataWritten();
		Pim = volume.GetPim ();
	}

    void VolumeInfo::Print() const {
        std::cerr << "Printing VolumeInfo - ";
        std::cerr << " | ProgramVersion: " << this->ProgramVersion;
        std::wcerr << " | AuxMountPoint: " << wstring(this->AuxMountPoint);
        std::cerr << " | EncryptionAlgorithmBlockSize: " << this->EncryptionAlgorithmBlockSize;
        std::cerr << " | EncryptionAlgorithmKeySize: " << this->EncryptionAlgorithmKeySize;
        std::cerr << " | EncryptionAlgorithmMinBlockSize: " << this->EncryptionAlgorithmMinBlockSize;
        std::wcerr << " | EncryptionAlgorithmName: " << wstring(this->EncryptionAlgorithmName);
        std::wcerr << " | EncryptionModeName: " << wstring(this->EncryptionModeName);
        std::cerr << " | HeaderCreationTime: " << this->HeaderCreationTime;
        std::cerr << " | HiddenVolumeProtectionTriggered: " << this->HiddenVolumeProtectionTriggered;
        std::wcerr << " | LoopDevice: " << wstring(this->LoopDevice);
        std::cerr << " | MinRequiredProgramVersion: " << this->MinRequiredProgramVersion;
        std::wcerr << " | MountPoint: " << wstring(this->MountPoint);
        std::wcerr << " | Path: " << wstring(this->Path);
        std::cerr << " | Pkcs5IterationCount: " << this->Pkcs5IterationCount;
        std::wcerr << " | Pkcs5PrfName: " << wstring(this->Pkcs5PrfName);
        std::cerr << " | Protection: " << static_cast <VolumeProtection::Enum> (this->Protection);
        std::cerr << " | SerialInstanceNumber: " << this->SerialInstanceNumber;
        std::cerr << " | Size: " << this->Size;
        std::cerr << " | SlotNumber: " << this->SlotNumber;
        std::cerr << " | SystemEncryption: " << this->SystemEncryption;
        std::cerr << " | TopWriteOffset: " << this->TopWriteOffset;
        std::cerr << " | TotalDataRead: " << this->TotalDataRead;
        std::cerr << " | TotalDataWritten: " << this->TotalDataWritten;
        std::cerr << " | Type: " << static_cast <VolumeType::Enum> (this->Type);
        std::wcerr << " | VirtualDevice: " << wstring(this->VirtualDevice);
        std::cerr << " | VolumeCreationTime: " << this->VolumeCreationTime;
        std::cerr << " | Pim: " << this->Pim;
        std::cerr << std::endl;
    }

	TC_SERIALIZER_FACTORY_ADD_CLASS (VolumeInfo);
}
