from ctypes import *
from sys import getsizeof
import os

class timeval(Structure):
    _fields_ = [("tv_sec",			c_long),
        ("tv_usec",			c_long)]

class v4l2_timecode(Structure):
    _fields_ = [("type",			c_uint32),
        ("flags",			c_uint32),
        ("frames",			c_uint8),
        ("seconds",			c_uint8),
        ("minutes",			c_uint8),
        ("hours",			c_uint8),
        ("userbits",			c_char * 4)]

class v4l2_plane_memory(Union):
    _fields_ = [("mem_offset",			c_uint32),
        ("userptr",			c_ulong),
        ("fd",			c_int32)]

class v4l2_plane(Structure):
    _fields_ = [("bytesused",			c_uint32),
        ("length",			c_uint32),
        ("m",			v4l2_plane_memory),
        ("data_offset",			c_uint32),
        ("reserved",			c_uint32 * 11)]

class v4l2_buffer_memory(Union):
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
    ("m",			v4l2_buffer_memory),
    ("length",			c_uint32),
    ("reserved2",			c_uint32),
	("request", request)]

exported_functions_video = [
    ("get_frame_user_ptr",
     [c_char_p], v4l2_buffer),
    ("open_device",
     [c_char_p], None),
    ("close_device",
     [c_char_p], None)]

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
    _libvideo = libvideo_loader(os.path.abspath("libvideo_thing.so"))

if __name__ == "__main__":
    load_video()
    with open("test.jpg", "wb") as file:
        device = "/dev/video0".encode('utf-8')
        _libvideo.open_device(device)
        print("get frame")
        frame = _libvideo.get_frame_user_ptr(device)
        file.write(bytearray([i.to_bytes(8, 'big') for i in cast(frame.m.userptr, POINTER(c_ulong))[:frame.bytesused//8]]))
        _libvideo.open_device(device)
