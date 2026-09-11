#!/usr/bin/env python3
"""Capture latest /cloud_map as colored binary PLY (meters, filtered).

Usage:
  python3 save_cloud.py --out /root/scans/scan.ply [--timeout 6] [--topic /cloud_map]

Wait --timeout seconds after the first cloud to let it settle, then write PLY.
Only RGB clouds (PointCloud2 XYZ + rgb) are supported; NaN points are dropped.
"""
import argparse
import os
import struct
import sys
import time

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2


def _fmt(pc):
    return {f.name: (f.offset, f.datatype) for f in pc.fields}


def write_ply(path, pc):
    pts = pc.width * pc.height
    fields = _fmt(pc)
    if 'x' not in fields or 'y' not in fields or 'z' not in fields:
        sys.stderr.write('cloud has no xyz fields\n')
        return 0
    off_x = fields['x'][0]
    off_y = fields['y'][0]
    off_z = fields['z'][0]
    rgb = fields.get('rgb', None)
    step = pc.point_step
    data = bytes(pc.data)
    tmp = path + '.tmp'
    count = 0
    finite = None
    with open(tmp, 'wb') as f:
        f.write(b'ply\nformat binary_little_endian 1.0\n')
        f.write(('element vertex %d\n' % pts).encode())
        f.write(b'property float x\nproperty float y\nproperty float z\n')
        if rgb is not None:
            f.write(b'property uchar red\nproperty uchar green\nproperty uchar blue\n')
        f.write(b'end_header\n')
        for i in range(pts):
            base = i * step
            if base + step > len(data):
                break
            xyz = struct.unpack_from('<fff', data, base + off_x)
            x, y, z = xyz[0], xyz[1], xyz[2]
            if not (x == x and y == y and z == z):  # NaN check
                continue
            if abs(x) > 1e6 or abs(y) > 1e6 or abs(z) > 1e6:
                continue
            if rgb is not None:
                v = struct.unpack_from('<I', data, base + rgb[0])[0]
                r = (v >> 16) & 0xFF
                g = (v >> 8) & 0xFF
                b = v & 0xFF
            else:
                r = g = b = 200
            f.write(struct.pack('<fffBBB', x, y, z, r, g, b))
            count += 1
        # fix header vertex count
    if count != pts:
        with open(tmp, 'rb') as f:
            blob = f.read()
        blob = blob.replace(('element vertex %d\n' % pts).encode(),
                            ('element vertex %d\n' % count).encode())
        with open(path, 'wb') as f:
            f.write(blob)
        os.unlink(tmp)
    else:
        os.rename(tmp, path)
    return count


class CloudSaver(Node):
    def __init__(self, topic):
        super().__init__('scan_saver')
        self.cloud = None
        self.sub = self.create_subscription(
            PointCloud2, topic, self.cb, 1)

    def cb(self, msg):
        self.cloud = msg


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', required=True)
    ap.add_argument('--timeout', type=float, default=6.0)
    ap.add_argument('--topic', default='/cloud_map')
    args = ap.parse_args()
    rclpy.init()
    node = CloudSaver(args.topic)
    deadline = time.time() + 15.0
    while node.cloud is None and time.time() < deadline:
        rclpy.spin_once(node, timeout_sec=0.2)
    if node.cloud is None:
        print('no cloud received on %s' % args.topic)
        node.destroy_node()
        rclpy.shutdown()
        sys.exit(1)
    # let the cloud settle to latest
    deadline = time.time() + args.timeout
    last = node.cloud
    while time.time() < deadline:
        rclpy.spin_once(node, timeout_sec=0.2)
        if node.cloud is not None and node.cloud.header.stamp != last.header.stamp:
            last = node.cloud
    os.makedirs(os.path.dirname(args.out) or '.', exist_ok=True)
    n = write_ply(args.out, last)
    print('saved %d points -> %s' % (n, args.out))
    node.destroy_node()
    rclpy.shutdown()
    sys.exit(0 if n else 1)


if __name__ == '__main__':
    main()