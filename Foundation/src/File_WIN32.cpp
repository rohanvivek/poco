//
// File_WIN32.cpp
//
// Library: Foundation
// Package: Filesystem
// Module:  File
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "Poco/File_WIN32.h"
#include "Poco/Exception.h"
#include "Poco/String.h"
#include "Poco/UnWindows.h"


namespace Poco {


class FileHandle
{
public:
	FileHandle(const std::string& path, DWORD access, DWORD share, DWORD disp)
	{
		_h = CreateFileA(path.c_str(), access, share, 0, disp, 0, 0);
		if (_h == INVALID_HANDLE_VALUE)
		{
			FileImpl::handleLastErrorImpl(path);
		}
	}

	~FileHandle()
	{
		if (_h != INVALID_HANDLE_VALUE) CloseHandle(_h);
	}

	HANDLE get() const
	{
		return _h;
	}

private:
	HANDLE _h;
};


FileImpl::FileImpl()
{
}


FileImpl::FileImpl(const std::string& path): _path(path)
{
	std::string::size_type n = _path.size();
	if (n > 1 && (_path[n - 1] == '\\' || _path[n - 1] == '/') && !((n == 3 && _path[1]==':')))
	{
		_path.resize(n - 1);
	}
}


FileImpl::~FileImpl()
{
}


void FileImpl::swapImpl(FileImpl& file)
{
	std::swap(_path, file._path);
}


void FileImpl::setPathImpl(const std::string& path)
{
	_path = path;
	std::string::size_type n = _path.size();
	if (n > 1 && (_path[n - 1] == '\\' || _path[n - 1] == '/') && !((n == 3 && _path[1]==':')))
	{
		_path.resize(n - 1);
	}
}


bool FileImpl::existsImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributes(_path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		switch (GetLastError())
		{
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_NOT_READY:
		case ERROR_INVALID_DRIVE:
			return false;
		default:
			handleLastErrorImpl(_path);
		}
	}
	return true;
}


bool FileImpl::canReadImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributes(_path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		switch (GetLastError())
		{
		case ERROR_ACCESS_DENIED:
			return false;
		default:
			handleLastErrorImpl(_path);
		}
	}
	return true;
}


bool FileImpl::canWriteImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributes(_path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
		handleLastErrorImpl(_path);
	return (attr & FILE_ATTRIBUTE_READONLY) == 0;
}


bool FileImpl::canExecuteImpl() const
{
	Path p(_path);
	return icompare(p.getExtension(), "exe") == 0;
}


bool FileImpl::isFileImpl() const
{
	return !isDirectoryImpl() && !isDeviceImpl();
}


bool FileImpl::isDirectoryImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributes(_path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
		handleLastErrorImpl(_path);
	return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}


bool FileImpl::isLinkImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributes(_path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
		handleLastErrorImpl(_path);
	return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0 && (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}



bool FileImpl::isDeviceImpl() const
{
	return
		_path.compare(0, 4, "\\\\.\\") == 0 ||
		icompare(_path, "CON") == 0 ||
		icompare(_path, "PRN") == 0 ||
		icompare(_path, "AUX") == 0 ||
		icompare(_path, "NUL") == 0 ||
		( (icompare(_path, 0, 3, "LPT") == 0 || icompare(_path, 0, 3, "COM") == 0) &&
		   _path.size() == 4 &&
		   _path[3] > 0x30   &&
		   isdigit(_path[3])
		);
}


bool FileImpl::isHiddenImpl() const
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributes(_path.c_str());
	if (attr == INVALID_FILE_ATTRIBUTES)
		handleLastErrorImpl(_path);
	return (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
}


Timestamp FileImpl::createdImpl() const
{
	poco_assert (!_path.empty());

	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesEx(_path.c_str(), GetFileExInfoStandard, &fad) == 0)
		handleLastErrorImpl(_path);
	return Timestamp::fromFileTimeNP(fad.ftCreationTime.dwLowDateTime, fad.ftCreationTime.dwHighDateTime);
}


Timestamp FileImpl::getLastModifiedImpl() const
{
	poco_assert (!_path.empty());

	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesEx(_path.c_str(), GetFileExInfoStandard, &fad) == 0)
		handleLastErrorImpl(_path);
	return Timestamp::fromFileTimeNP(fad.ftLastWriteTime.dwLowDateTime, fad.ftLastWriteTime.dwHighDateTime);
}


void FileImpl::setLastModifiedImpl(const Timestamp& ts)
{
	poco_assert (!_path.empty());

	UInt32 low;
	UInt32 high;
	ts.toFileTimeNP(low, high);
	FILETIME ft;
	ft.dwLowDateTime  = low;
	ft.dwHighDateTime = high;
	FileHandle fh(_path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING);
	if (SetFileTime(fh.get(), 0, &ft, &ft) == 0)
		handleLastErrorImpl(_path);
}


FileImpl::FileSizeImpl FileImpl::getSizeImpl() const
{
	poco_assert (!_path.empty());

	WIN32_FILE_ATTRIBUTE_DATA fad;
	if (GetFileAttributesEx(_path.c_str(), GetFileExInfoStandard, &fad) == 0)
		handleLastErrorImpl(_path);
	LARGE_INTEGER li;
	li.LowPart  = fad.nFileSizeLow;
	li.HighPart = fad.nFileSizeHigh;
	return li.QuadPart;
}


void FileImpl::setSizeImpl(FileSizeImpl size)
{
	poco_assert (!_path.empty());

	FileHandle fh(_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING);
	LARGE_INTEGER li;
	li.QuadPart = size;
	if (SetFilePointer(fh.get(), li.LowPart, &li.HighPart, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		handleLastErrorImpl(_path);
	if (SetEndOfFile(fh.get()) == 0)
		handleLastErrorImpl(_path);
}


void FileImpl::setWriteableImpl(bool flag)
{
	poco_assert (!_path.empty());

	DWORD attr = GetFileAttributes(_path.c_str());
	if (attr == -1)
		handleLastErrorImpl(_path);
	if (flag)
		attr &= ~FILE_ATTRIBUTE_READONLY;
	else
		attr |= FILE_ATTRIBUTE_READONLY;
	if (SetFileAttributes(_path.c_str(), attr) == 0)
		handleLastErrorImpl(_path);
}


void FileImpl::setExecutableImpl(bool flag)
{
	// not supported
}


void FileImpl::copyToImpl(const std::string& path, int options) const
{
	poco_assert (!_path.empty());

	if (CopyFileA(_path.c_str(), path.c_str(), (options & OPT_FAIL_ON_OVERWRITE_IMPL) != 0) == 0)
		handleLastErrorImpl(_path);
}


void FileImpl::renameToImpl(const std::string& path, int options)
{
	poco_assert (!_path.empty());

	if (options & OPT_FAIL_ON_OVERWRITE_IMPL) {
		if (MoveFileExA(_path.c_str(), path.c_str(), NULL) == 0)
			handleLastErrorImpl(_path);
	} else {
		if (MoveFileExA(_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
			handleLastErrorImpl(_path);
	}
	
}


void FileImpl::linkToImpl(const std::string& path, int type) const
{
	poco_assert (!_path.empty());

	if (type == 0)
	{
		if (CreateHardLinkA(path.c_str(), _path.c_str(), NULL) == 0)
			handleLastErrorImpl(_path);
	}
	else
	{
#if _WIN32_WINNT >= 0x0600 && defined(SYMBOLIC_LINK_FLAG_DIRECTORY)
		DWORD flags = 0;
		if (isDirectoryImpl()) flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
		flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
		if (CreateSymbolicLinkA(path.c_str(), _path.c_str(), flags) == 0)
			handleLastErrorImpl(_path);
#else
		throw Poco::NotImplementedException("Symbolic link support not available in used version of the Windows SDK")
#endif

	}
}


void FileImpl::removeImpl()
{
	poco_assert (!_path.empty());

	if (isDirectoryImpl())
	{
		if (RemoveDirectoryA(_path.c_str()) == 0)
			handleLastErrorImpl(_path);
	}
	else
	{
		if (DeleteFileA(_path.c_str()) == 0)
			handleLastErrorImpl(_path);
	}
}


bool FileImpl::createFileImpl()
{
	poco_assert (!_path.empty());

	HANDLE hFile = CreateFileA(_path.c_str(), GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hFile);
		return true;
	}
	else if (GetLastError() == ERROR_FILE_EXISTS)
		return false;
	else
		handleLastErrorImpl(_path);
	return false;
}


bool FileImpl::createDirectoryImpl()
{
	poco_assert (!_path.empty());

	if (existsImpl() && isDirectoryImpl())
		return false;
	if (CreateDirectoryA(_path.c_str(), 0) == 0)
		handleLastErrorImpl(_path);
	return true;
}


FileImpl::FileSizeImpl FileImpl::totalSpaceImpl() const
{
	poco_assert(!_path.empty());

	ULARGE_INTEGER space;
	if (!GetDiskFreeSpaceExA(_path.c_str(), NULL, &space, NULL))
		handleLastErrorImpl(_path);
	return space.QuadPart;
}


FileImpl::FileSizeImpl FileImpl::usableSpaceImpl() const
{
	poco_assert(!_path.empty());

	ULARGE_INTEGER space;
	if (!GetDiskFreeSpaceExA(upath.c_str(), &space, NULL, NULL))
		handleLastErrorImpl(_path);
	return space.QuadPart;
}


FileImpl::FileSizeImpl FileImpl::freeSpaceImpl() const
{
	poco_assert(!_path.empty());

	ULARGE_INTEGER space;
	if (!GetDiskFreeSpaceExA(_path.c_str(), NULL, NULL, &space))
		handleLastErrorImpl(_path);
	return space.QuadPart;
}


void FileImpl::handleLastErrorImpl(const std::string& path)
{
	DWORD err = GetLastError();
	switch (err)
	{
	case ERROR_FILE_NOT_FOUND:
		throw FileNotFoundException(path, err);
	case ERROR_PATH_NOT_FOUND:
	case ERROR_BAD_NETPATH:
	case ERROR_CANT_RESOLVE_FILENAME:
	case ERROR_INVALID_DRIVE:
		throw PathNotFoundException(path, err);
	case ERROR_ACCESS_DENIED:
		throw FileAccessDeniedException(path, err);
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		throw FileExistsException(path, err);
	case ERROR_INVALID_NAME:
	case ERROR_DIRECTORY:
	case ERROR_FILENAME_EXCED_RANGE:
	case ERROR_BAD_PATHNAME:
		throw PathSyntaxException(path, err);
	case ERROR_FILE_READ_ONLY:
		throw FileReadOnlyException(path, err);
	case ERROR_CANNOT_MAKE:
		throw CreateFileException(path, err);
	case ERROR_DIR_NOT_EMPTY:
		throw DirectoryNotEmptyException(path, err);
	case ERROR_WRITE_FAULT:
		throw WriteFileException(path, err);
	case ERROR_READ_FAULT:
		throw ReadFileException(path, err);
	case ERROR_SHARING_VIOLATION:
		throw FileException("sharing violation", path, err);
	case ERROR_LOCK_VIOLATION:
		throw FileException("lock violation", path, err);
	case ERROR_HANDLE_EOF:
		throw ReadFileException("EOF reached", path, err);
	case ERROR_HANDLE_DISK_FULL:
	case ERROR_DISK_FULL:
		throw WriteFileException("disk is full", path, err);
	case ERROR_NEGATIVE_SEEK:
		throw FileException("negative seek", path, err);
	default:
		throw FileException(path, err);
	}
}


} // namespace Poco
