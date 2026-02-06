import gi
import threading
gi.require_version('Gst', '1.0')
from gi.repository import Gst

Gst.init(None)

def start_encode(source_str, output_file): #encode using gst
    pipeline_str = (
        f'{source_str} ! '
        'video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! '
        'nvvidconv ! nvv4l2h265enc bitrate=4000000 ! h265parse ! qtmux ! filesink location=' + output_file
    )
    pipeline = Gst.parse_launch(pipeline_str)
    pipeline.set_state(Gst.State.PLAYING)

sources = [
    'nvarguscamerasrc sensor-id=0', #CSI Cameras
    'nvarguscamerasrc sensor-id=1',
    'v4l2src device=/dev/video0', #USB Cameras
    'v4l2src device=/dev/video1'
]

threads = []
for i, src in enumerate(sources): #encode each thread
    t = threading.Thread(target=start_encode, args=(src, f'output_{i}.mp4'))
    t.start()
    threads.append(t)

for t in threads: # join threads (if needed)
    t.join()
