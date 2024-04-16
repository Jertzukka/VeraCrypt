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

#include "CoreUnix.h"
#include <errno.h>
#include <iostream>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include "Platform/FileStream.h"
#include "Driver/Fuse/FuseService.h"
#include "Volume/VolumePasswordCache.h"
#include "Platform/SystemLog.h"
#include "Core/Core.h"
#include <algorithm>

namespace VeraCrypt
{
#ifdef TC_LINUX
	static string GetTmpUser ();
	static bool SamePath (const string& path1, const string& path2);
#endif

	CoreUnix::CoreUnix ()
	{
		signal (SIGPIPE, SIG_IGN);

		char *loc = setlocale (LC_ALL, "");
		if (!loc || string (loc) == "C")
			setlocale (LC_ALL, "en_US.UTF-8");
	}

	CoreUnix::~CoreUnix ()
	{
	}

	void CoreUnix::CheckFilesystem (shared_ptr <VolumeInfo> mountedVolume, bool repair) const
	{
		if (!mountedVolume->MountPoint.IsEmpty())
			DismountFilesystem (mountedVolume->MountPoint, false);

		list <string> args;

		args.push_back ("-T");
		args.push_back ("fsck");

		args.push_back ("-e");

		string xargs = "fsck ";

#ifdef TC_LINUX
		if (!repair)
			xargs += "-n ";
		else
			xargs += "-r ";
#endif

		xargs += string (mountedVolume->VirtualDevice) + "; echo '[Done]'; read W";
		args.push_back (xargs);

		try
		{
			Process::Execute ("xterm", args, 1000);
		} catch (TimeOut&) { }
#ifdef TC_LINUX
		catch (SystemException&)
		{
			// xterm not available. Try with KDE konsole if it exists
			struct stat sb;
			if (stat("/usr/bin/konsole", &sb) == 0)
			{
				args.clear ();
				args.push_back ("-p");
				args.push_back ("tabtitle=fsck");
				args.push_back ("-e");
				args.push_back ("sh");
				args.push_back ("-c");
				args.push_back (xargs);
				try
				{
					Process::Execute ("konsole", args, 1000);
				} catch (TimeOut&) { }
			}
			else if (stat("/usr/bin/gnome-terminal", &sb) == 0 && stat("/usr/bin/dbus-launch", &sb) == 0)
			{
				args.clear ();
				args.push_back ("--title");
				args.push_back ("fsck");
				args.push_back ("--");
				args.push_back ("sh");
				args.push_back ("-c");
				args.push_back (xargs);
				try
				{
					Process::Execute ("gnome-terminal", args, 1000);
				} catch (TimeOut&) { }
			}
			else
				throw TerminalNotFound();
		}
#endif
	}

	void CoreUnix::DismountFilesystem (const DirectoryPath &mountPoint, bool force) const
	{
		list <string> args;

#ifdef TC_MACOSX
		if (force)
			args.push_back ("-f");
#endif
		args.push_back ("--");
		args.push_back (mountPoint);

		Process::Execute ("umount", args);
	}

	shared_ptr <VolumeInfo> CoreUnix::DismountVolume (shared_ptr <VolumeInfo> mountedVolume, bool ignoreOpenFiles, bool syncVolumeInfo)
	{
		if (!mountedVolume->MountPoint.IsEmpty())
		{
			DismountFilesystem (mountedVolume->MountPoint, ignoreOpenFiles);

			// Delete mount directory if a default path has been used
			if (string (mountedVolume->MountPoint).find (GetDefaultMountPointPrefix()) == 0)
				mountedVolume->MountPoint.Delete();
		}

		try
		{
			DismountNativeVolume (mountedVolume);
		}
		catch (NotApplicable &) { }

		if (!mountedVolume->LoopDevice.IsEmpty())
		{
			try
			{
				DetachLoopDevice (mountedVolume->LoopDevice);
			}
			catch (ExecutedProcessFailed&) { }
		}

		if (syncVolumeInfo || mountedVolume->Protection == VolumeProtection::HiddenVolumeReadOnly)
		{
			sync();
			VolumeInfoList ml = GetMountedVolumes (mountedVolume->Path);

			if (ml.size() > 0)
				mountedVolume = ml.front();
		}

		list <string> args;
		args.push_back ("--");
		args.push_back (mountedVolume->AuxMountPoint);

		for (int t = 0; true; t++)
		{
			try
			{
				Process::Execute ("umount", args);
				break;
			}
			catch (ExecutedProcessFailed&)
			{
				if (t > 10)
					throw;
				Thread::Sleep (200);
			}
		}

		try
		{
			mountedVolume->AuxMountPoint.Delete();
		}
		catch (...)	{ }

		VolumeEventArgs eventArgs (mountedVolume);
		VolumeDismountedEvent.Raise (eventArgs);

		return mountedVolume;
	}

	void CoreUnix::RemoveDevMapper (const string& devMapName) const
	{
		stringstream pathss;
		pathss << "Core Service received request to remove dev mapper: " << devMapName << std::endl;
		SystemLog::WriteError(pathss.str());

		list <string> dmsetupArgs;
		dmsetupArgs.push_back ("remove");
		dmsetupArgs.push_back (StringConverter::Split (devMapName, "/").back());

		for (int t = 0; true; t++)
		{
			try
			{
				Process::Execute ("dmsetup", dmsetupArgs);
				break;
			}
			catch (...)
			{
				if (t > 20)
					throw;

				Thread::Sleep (100);
			}
		}
	}

	bool CoreUnix::FilesystemSupportsLargeFiles (const FilePath &filePath) const
	{
		string path = filePath;
		size_t pos;

		while ((pos = path.find_last_of ('/')) != string::npos)
		{
			path = path.substr (0, pos);

			if (path.empty())
				break;

			try
			{
				MountedFilesystemList filesystems = GetMountedFilesystems (DevicePath(), path);
				if (!filesystems.empty())
				{
					const MountedFilesystem &fs = *filesystems.front();

					if (fs.Type == "fat"
						|| fs.Type == "fat32"
						|| fs.Type == "vfat"
						|| fs.Type == "fatfs"
						|| fs.Type == "msdos"
						|| fs.Type == "msdosfs"
						|| fs.Type == "umsdos"
						|| fs.Type == "dos"
						|| fs.Type == "dosfs"
						|| fs.Type == "pcfs"
						)
					{
						return false;
					}

					return true;
				}
			}
			catch (...) { }
		}

		return true;	// Prevent errors if the filesystem cannot be identified
	}

	bool CoreUnix::FilesystemSupportsUnixPermissions (const DevicePath &devicePath) const
	{
		File device;
		device.Open (devicePath);

		Buffer bootSector (device.GetDeviceSectorSize());
		device.SeekAt (0);
		device.ReadCompleteBuffer (bootSector);

		byte *b = bootSector.Ptr();

		return memcmp (b + 3,  "NTFS", 4) != 0
			&& memcmp (b + 54, "FAT", 3) != 0
			&& memcmp (b + 82, "FAT32", 5) != 0
			&& memcmp (b + 3,  "EXFAT", 5) != 0;
	}

	string CoreUnix::GetDefaultMountPointPrefix () const
	{
		const char *envPrefix = getenv ("VERACRYPT_MOUNT_PREFIX");
		if (envPrefix && !string (envPrefix).empty())
			return envPrefix;

		if (FilesystemPath ("/media").IsDirectory())
			return "/media/veracrypt";

		if (FilesystemPath ("/mnt").IsDirectory())
			return "/mnt/veracrypt";

		return GetTempDirectory() + "/veracrypt_mnt";
	}

	uint32 CoreUnix::GetDeviceSectorSize (const DevicePath &devicePath) const
	{
		File dev;
		dev.Open (devicePath);
		return dev.GetDeviceSectorSize();
	}

	uint64 CoreUnix::GetDeviceSize (const DevicePath &devicePath) const
	{
		File dev;
		dev.Open (devicePath);
		return dev.Length();
	}

	DirectoryPath CoreUnix::GetDeviceMountPoint (const DevicePath &devicePath) const
	{
		DevicePath devPath = devicePath;
#ifdef TC_MACOSX
		if (string (devPath).find ("/dev/rdisk") != string::npos)
			devPath = string ("/dev/") + string (devicePath).substr (6);
#endif
		MountedFilesystemList mountedFilesystems = GetMountedFilesystems (devPath);

		if (mountedFilesystems.size() < 1)
			return DirectoryPath();

		return mountedFilesystems.front()->MountPoint;
	}

	VolumeInfoList CoreUnix::GetMountedVolumes (const VolumePath &volumePath, bool throwing) const
	{
		std::cerr << "GetMountedVolumes (" << string(volumePath) << ")" << std::endl;
		VolumeInfoList volumes;

		foreach_ref (const MountedFilesystem &mf, GetMountedFilesystems ())
		{
			if (string (mf.MountPoint).find (GetFuseMountDirPrefix()) == string::npos)
				continue;
			std::cerr << "FUSE prefix found on Device: " << string(mf.Device) << " MountPoint: " << string(mf.MountPoint) << " Type: " << string(mf.Type) << std::endl;

			shared_ptr <VolumeInfo> mountedVol;
			shared_ptr <File> controlFile (new File);
			try {
				controlFile->Open (string (mf.MountPoint) + FuseService::GetControlPath());
			}
			catch (...)
			{
				if (throwing) {
					static bool thrownalready = false;
					if (!thrownalready) {
						thrownalready = true;
						throw MountsWithoutControls(SRC_POS);
					}
				} else continue;
			}

			try {
				shared_ptr <Stream> controlFileStream (new FileStream (controlFile));
				mountedVol = Serializable::DeserializeNew <VolumeInfo> (controlFileStream);
			}
			catch (...)
			{
				continue;
			}

			if (!volumePath.IsEmpty() && wstring (mountedVol->Path).compare (volumePath) != 0)
				continue;

			mountedVol->AuxMountPoint = mf.MountPoint;

			if (!mountedVol->VirtualDevice.IsEmpty())
			{
				MountedFilesystemList mpl = GetMountedFilesystems (mountedVol->VirtualDevice);

				if (mpl.size() > 0)
					mountedVol->MountPoint = mpl.front()->MountPoint;
			}

			volumes.push_back (mountedVol);
            mountedVol->Print();

            if (!volumePath.IsEmpty())
				break;
		}

		return volumes;
	}

	gid_t CoreUnix::GetRealGroupId () const
	{
		const char *env = getenv ("SUDO_GID");
		if (env)
		{
			try
			{
				string s (env);
				return static_cast <gid_t> (StringConverter::ToUInt64 (s));
			}
			catch (...) { }
		}

		return getgid();
	}

	uid_t CoreUnix::GetRealUserId () const
	{
		const char *env = getenv ("SUDO_UID");
		if (env)
		{
			try
			{
				string s (env);
				return static_cast <uid_t> (StringConverter::ToUInt64 (s));
			}
			catch (...) { }
		}

		return getuid();
	}

	string CoreUnix::GetTempDirectory () const
	{
		const char *tmpdir = getenv ("TMPDIR");
		string envDir = tmpdir ? tmpdir : "/tmp";

#ifdef TC_LINUX
		/*
		 * If pam_tmpdir.so is in use, a different temporary directory is
		 * allocated for each user ID. We need to mount to the directory used
		 * by the non-root user.
		 */
		if (getuid () == 0 && envDir.size () >= 2
			&& envDir.substr (envDir.size () - 2) == "/0") {
			string tmpuser = GetTmpUser ();
			if (SamePath (envDir, tmpuser + "/0")) {
				/* Substitute the sudo'ing user for 0 */
				char uid[40];
				FILE *fp = fopen ("/proc/self/loginuid", "r");
				if (fp != NULL) {
					if (fgets (uid, sizeof (uid), fp) != nullptr) {
						envDir = tmpuser + "/" + uid;
					}
					fclose (fp);
				}
			}
		}
#endif

		return envDir;
	}

#ifdef TC_LINUX
	static string GetTmpUser ()
	{
		string tmpuser = "/tmp/user";
		FILE *fp = fopen ("/etc/security/tmpdir.conf", "r");
		if (fp == NULL) {
			return tmpuser;
		}
		while (true) {
			/* Parses the same way as pam_tmpdir */
			char line[1024];
			if (fgets (line, sizeof (line), fp) == nullptr) {
				break;
			}
			if (line[0] == '#') {
				continue;
			}
			size_t len = strlen (line);
			if (len > 0 && line[len-1] == '\n') {
				line[len-1] = '\0';
			}
			char *eq = strchr (line, '=');
			if (eq == nullptr) {
				continue;
			}
			*eq = '\0';
			const char *key = line;
			const char *value = eq + 1;
			if (strcmp (key, "tmpdir") == 0) {
				tmpuser = value;
				break;
			}
		}
		fclose (fp);
		return tmpuser;
	}

	static bool SamePath (const string& path1, const string& path2)
	{
		size_t i1 = 0;
		size_t i2 = 0;
		while (i1 < path1.size () && i2 < path2.size ()) {
			if (path1[i1] != path2[i2]) {
				return false;
			}
			/* Any two substrings consisting entirely of slashes compare equal */
			if (path1[i1] == '/') {
				while (i1 < path1.size () && path1[i1] == '/') {
					++i1;
				}
				while (i2 < path2.size () && path2[i2] == '/') {
					++i2;
				}
			}
			else
			{
				++i1;
				++i2;
			}
		}
		return (i1 == path1.size () && i2 == path2.size ());
	}
#endif

	bool CoreUnix::IsMountPointAvailable (const DirectoryPath &mountPoint) const
	{
		return GetMountedFilesystems (DevicePath(), mountPoint).size() == 0;
	}

	void CoreUnix::MountFilesystem (const DevicePath &devicePath, const DirectoryPath &mountPoint, const string &filesystemType, bool readOnly, const string &systemMountOptions) const
	{
		if (GetMountedFilesystems (DevicePath(), mountPoint).size() > 0)
			throw MountPointUnavailable (SRC_POS);

		list <string> args;
		string options;

		if (!filesystemType.empty())
		{
#ifdef TC_SOLARIS
			args.push_back ("-F");
#else
			args.push_back ("-t");
#endif
			args.push_back (filesystemType);
		}

		if (readOnly)
			options = "-oro";

		if (!systemMountOptions.empty())
		{
			if (options.empty())
				options = "-o";
			else
				options += ",";

			options += systemMountOptions;
		}

		if (!options.empty())
			args.push_back (options);

		args.push_back ("--");
		args.push_back (devicePath);
		args.push_back (mountPoint);

		Process::Execute ("mount", args);
	}

	VolumeSlotNumber CoreUnix::MountPointToSlotNumber (const DirectoryPath &mountPoint) const
	{
		string mountPointStr (mountPoint);
		if (mountPointStr.find (GetDefaultMountPointPrefix()) == 0)
		{
			try
			{
				return StringConverter::ToUInt32 (StringConverter::GetTrailingNumber (mountPointStr));
			}
			catch (...) { }
		}
		return GetFirstFreeSlotNumber();
	}

    void CoreUnix::DismountMountsWithoutControls(const VolumeInfoList volumes) const {
        //if (!Gui->AskYesNo ("It appears that your FUSE control files have been lost. Want to dismount these devices?", true, true))
        //    return;
        //Core->WarningEvent.Raise (args);

        std::cerr << "Attempting to dismount all VeraCrypt mounts without corresponding FUSE control files." << std::endl;
        std::vector<string> auxmntstokeep = {};
        std::vector<string> virtualdevicestokeep = {};

        std::vector<string> auxmntstoremove = {};
        std::vector<string> virtualdevicestoremove = {};

        foreach_ref (const VolumeInfo &volumeinfo, volumes) {
                auxmntstokeep.push_back(volumeinfo.AuxMountPoint);
                virtualdevicestokeep.push_back(volumeinfo.VirtualDevice);
                std::cerr << "Volume set to keep: AuxMountPoint: " << string(volumeinfo.AuxMountPoint) << ", VirtualDevice: " << string(volumeinfo.VirtualDevice) << std::endl;
            }

        MountedFilesystemList mountedfslist = GetMountedFilesystems();
        foreach_ref (const MountedFilesystem &mountedfs, mountedfslist) {
                string virtualdevice = mountedfs.Device;
                string mountpoint = mountedfs.MountPoint;

                if (virtualdevice.find("/dev/mapper/veracrypt") == 0) {
                    if (std::find(virtualdevicestokeep.begin(), virtualdevicestokeep.end(), virtualdevice) == virtualdevicestokeep.end()) {
                        virtualdevicestoremove.push_back(virtualdevice);
                        std::cerr << "VirtualDevice " << virtualdevice << " set to be dismounted." << std::endl;
                    }
                    continue;
                }

                if (mountpoint.find("/tmp/.veracrypt_aux_mnt") == 0) {
                    if (std::find(auxmntstokeep.begin(), auxmntstokeep.end(), mountpoint) == auxmntstokeep.end()) {
                        auxmntstoremove.push_back(mountpoint);
                        std::cerr << "AuxMountPoint " << mountpoint << " set to be dismounted." << std::endl;
                    }
                    continue;
                }
            }

        foreach (const string &virtualdevice, virtualdevicestoremove) {
                std::cerr << "Dismounting Virtual Device: " << virtualdevice << std::endl;
                DismountFilesystem(virtualdevice, false);
                std::cerr << "Removing DevMapper for: " << virtualdevice << std::endl;
                RemoveDevMapper(virtualdevice);
            }

        foreach (const string &auxmnt, auxmntstoremove) {
                std::cerr << "Dismounting AuxMountPoint: " << auxmnt << std::endl;
                DismountFilesystem(auxmnt, false);
            }
    }

	shared_ptr <VolumeInfo> CoreUnix::MountVolume (MountOptions &options)
	{
        stringstream pathss;
        pathss << "CoreUnix::MountVolume (options " << options.Path << ")" << std::endl;
        SystemLog::WriteDebug(pathss.str());

		CoalesceSlotNumberAndMountPoint (options);

		if (IsVolumeMounted (*options.Path))
			throw VolumeAlreadyMounted (SRC_POS);

		Cipher::EnableHwSupport (!options.NoHardwareCrypto);

		shared_ptr <Volume> volume;

		while (true)
		{
			try
			{
				volume = OpenVolume (
					options.Path,
					options.PreserveTimestamps,
					options.Password,
					options.Pim,
					options.Kdf,
					options.Keyfiles,
					options.EMVSupportEnabled,
					options.Protection,
					options.ProtectionPassword,
					options.ProtectionPim,
					options.ProtectionKdf,
					options.ProtectionKeyfiles,
					options.SharedAccessAllowed,
					VolumeType::Unknown,
					options.UseBackupHeaders,
					options.PartitionInSystemEncryptionScope
					);

				options.Password.reset();
			}
			catch (SystemException &e)
			{
				if (options.Protection != VolumeProtection::ReadOnly
					&& (e.GetErrorCode() == EROFS || e.GetErrorCode() == EACCES || e.GetErrorCode() == EPERM))
				{
					// Read-only filesystem
					options.Protection = VolumeProtection::ReadOnly;
					continue;
				}

				options.Password.reset();
				throw;
			}

			break;
		}

		if (options.Path->IsDevice())
		{
			const uint32 devSectorSize = volume->GetFile()->GetDeviceSectorSize();
			const size_t volSectorSize = volume->GetSectorSize();
			if (devSectorSize != volSectorSize)
				throw DeviceSectorSizeMismatch (SRC_POS, StringConverter::ToWide(devSectorSize) + L" != " + StringConverter::ToWide((uint32) volSectorSize));
		}

		// Find a free mount point for FUSE service
		MountedFilesystemList mountedFilesystems = GetMountedFilesystems ();
		string fuseMountPoint;
		for (int i = 1; true; i++)
		{
			stringstream path;
			path << GetTempDirectory() << "/" << GetFuseMountDirPrefix() << i;
			FilesystemPath fsPath (path.str());

			bool inUse = false;

			foreach_ref (const MountedFilesystem &mf, mountedFilesystems)
			{
				if (mf.MountPoint == path.str())
				{
					inUse = true;
					break;
				}
			}

			if (!inUse)
			{
				try
				{
					if (fsPath.IsDirectory())
						fsPath.Delete();

					throw_sys_sub_if (mkdir (path.str().c_str(), S_IRUSR | S_IXUSR) == -1, path.str());

					fuseMountPoint = fsPath;
					break;
				}
				catch (...)
				{
					if (i > 255)
						throw TemporaryDirectoryFailure (SRC_POS, StringConverter::ToWide (path.str()));
				}
			}
		}

		try
		{
			FuseService::Mount (volume, options.SlotNumber, fuseMountPoint);
		}
		catch (...)
		{
			try
			{
				DirectoryPath (fuseMountPoint).Delete();
			}
			catch (...) { }
			throw;
		}

		try
		{
			// Create a mount directory if a default path has been specified
			bool mountDirCreated = false;
			string mountPoint;
			if (!options.NoFilesystem && options.MountPoint)
			{
				mountPoint = *options.MountPoint;

#ifndef TC_MACOSX
				if (mountPoint.find (GetDefaultMountPointPrefix()) == 0 && !options.MountPoint->IsDirectory())
				{
					Directory::Create (*options.MountPoint);
					try
					{
						throw_sys_sub_if (chown (mountPoint.c_str(), GetRealUserId(), GetRealGroupId()) == -1, mountPoint);
					} catch (ParameterIncorrect&) { }

					mountDirCreated = true;
				}
#endif
			}

			try
			{
				try
				{
					MountVolumeNative (volume, options, fuseMountPoint);
				}
				catch (NotApplicable&)
				{
					MountAuxVolumeImage (fuseMountPoint, options);
				}
			}
			catch (...)
			{
				if (mountDirCreated)
					remove (mountPoint.c_str());
				throw;
			}

#ifndef TC_MACOSX
			// set again correct ownership of the mount point to avoid any issues
			if (!options.NoFilesystem && options.MountPoint)
			{
				mountPoint = *options.MountPoint;

				if (mountPoint.find (GetDefaultMountPointPrefix()) == 0)
				{
					try
					{
						throw_sys_sub_if (chown (mountPoint.c_str(), GetRealUserId(), GetRealGroupId()) == -1, mountPoint);
					} catch (...) { }
				}
			}
#endif

		}
		catch (...)
		{
			try
			{
				VolumeInfoList mountedVolumes = GetMountedVolumes (*options.Path);
				if (mountedVolumes.size() > 0)
				{
					shared_ptr <VolumeInfo> mountedVolume (mountedVolumes.front());
					DismountVolume (mountedVolume);
				}
			}
			catch (...) { }
			throw;
		}

		VolumeInfoList mountedVolumes = GetMountedVolumes (*options.Path);
		if (mountedVolumes.size() != 1)
			throw ParameterIncorrect (SRC_POS);

		VolumeEventArgs eventArgs (mountedVolumes.front());
		VolumeMountedEvent.Raise (eventArgs);

		return mountedVolumes.front();
	}

	void CoreUnix::MountAuxVolumeImage (const DirectoryPath &auxMountPoint, const MountOptions &options) const
	{
        stringstream pathss;
        pathss << "CoreUnix::MountAuxVolumeImage (auxMountPoint " << string(auxMountPoint) << ", options " << options.Path << ")" << std::endl;
        SystemLog::WriteDebug(pathss.str());

		DevicePath loopDev = AttachFileToLoopDevice (string (auxMountPoint) + FuseService::GetVolumeImagePath(), options.Protection == VolumeProtection::ReadOnly);

		try
		{
			FuseService::SendAuxDeviceInfo (auxMountPoint, loopDev, loopDev);
		}
		catch (...)
		{
			try
			{
				DetachLoopDevice (loopDev);
			}
			catch (...) { }
			throw;
		}

		if (!options.NoFilesystem && options.MountPoint && !options.MountPoint->IsEmpty())
		{
			MountFilesystem (loopDev, *options.MountPoint,
				StringConverter::ToSingle (options.FilesystemType),
				options.Protection == VolumeProtection::ReadOnly,
				StringConverter::ToSingle (options.FilesystemOptions));
		}
	}

	void CoreUnix::SetFileOwner (const FilesystemPath &path, const UserId &owner) const
	{
		throw_sys_if (chown (string (path).c_str(), owner.SystemId, (gid_t) -1) == -1);
	}

	DirectoryPath CoreUnix::SlotNumberToMountPoint (VolumeSlotNumber slotNumber) const
	{
		if (slotNumber < GetFirstSlotNumber() || slotNumber > GetLastSlotNumber())
			throw ParameterIncorrect (SRC_POS);

		stringstream s;
		s << GetDefaultMountPointPrefix() << slotNumber;
		return s.str();
	}
}
