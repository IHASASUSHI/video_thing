from ctypes import *

class timeval(Structure):
    pass

class v4l2_timecode(Structure):
    pass

class v4l2_plane(Structure):
    pass

class memory(Union):
    _fields_ = [("offset",			c_uint32),
        ("userptr",			c_ulong),
        ("planes",			POINTER(v4l2_plane)),
        ("fd",			c_int32)]

class request(Union):
    _fields_ = [("request_fd",			c_int32),
        ("reserved",			c_uint32)]

class v4l2_buffer(Structure):
    _fields_ = [("index",			c_uint32),
    ("type",			c_uint32),
    ("bytesused",			c_uint32),
    ("flags",			c_uint32),
    ("field",			c_uint32),
    ("timestamp", timeval),
    ("timecode", v4l2_timecode),
    ("sequence",			c_uint32),
    ("memory",			c_uint32),
    ("m",			memory),
    ("length",			c_uint32),
    ("reserved2",			c_uint32),
	("request", request)]

exported_functions_video = [
    ("get_frame",
     [c_int], v4l2_buffer),
    ("open_device",
     [c_wchar_p], None),
    ("close_device",
     [c_wchar_p], None)]

def libvideo_loader(name):
    lib = cdll.LoadLibrary(name)

    # registering functions
    for item in exported_functions_video:
        func = getattr(lib, item[0])

        try:
            if item[1]:
                func.argtypes = item[1]

            func.restype = item[2]
        except KeyError:
            pass

    return lib


def load_video():
    global _libvideo
    _libvideo = libvideo_loader("/home/bright/video_thing/libvideo_thing.so")

if __name__ == "__main__":
    load_video()
    with open("test.jpg", "wb") as file:
        _libvideo.open_device(c_wchar_p("/dev/video0"))
        frame = _libvideo.get_frame(c_wchar_p("/dev/video0"))
        file.write(frame.m.userptr[frame.bytesused])
        _libvideo.open_device(c_wchar_p("/dev/video0"))
