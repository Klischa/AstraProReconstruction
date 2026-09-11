#!/usr/bin/env python3
"""Crop-box point cloud filter for object scanning.

Subscribes to the accumulated map cloud (default /rtabmap/cloud_map) and
publishes only the points that fall inside a user-definable 3D box:

    /scan/box_points        - filtered (cropped) colored point cloud
    /scan_box/marker        - visualization_msgs/Marker (wireframe-ish cube)

The box can be moved and resized live via parameters on /scan_box_filter:

    center_x/y/z   box center (meters, in map frame)
    size_x/y/z     box size (meters)

Also accepts explicit bounds min_x/min_y/... which override the box center.
"""
import math
import struct

import numpy as np
import rclpy
from rcl_interfaces.msg import SetParametersResult
from rclpy.node import Node
from sensor_msgs.msg import PointCloud2, PointField
from visualization_msgs.msg import Marker

F32 = PointField.FLOAT32
U8 = PointField.UINT8
U32 = PointField.UINT32

FIELDS_XYZ = [('x', F32), ('y', F32), ('z', F32)]
FIELDS_RGB = [('rgb', U32)]

# Compatibility field name used by old rtabmap cloud maps
RGB_NAMES = ('rgb', 'colors')


class ScanBoxFilter(Node):

    def __init__(self):
        super().__init__('scan_box_filter')

        self.declare_parameter('input_topic', '/rtabmap/cloud_map')
        self.declare_parameter('output_topic', '/scan/box_points')
        self.declare_parameter('marker_frame', 'map')

        self.declare_parameter('center_x', 0.0)
        self.declare_parameter('center_y', 0.0)
        self.declare_parameter('center_z', 0.7)
        self.declare_parameter('size_x', 0.8)
        self.declare_parameter('size_y', 0.8)
        self.declare_parameter('size_z', 0.8)

        self.declare_parameter('min_x', float('nan'))
        self.declare_parameter('min_y', float('nan'))
        self.declare_parameter('min_z', float('nan'))
        self.declare_parameter('max_x', float('nan'))
        self.declare_parameter('max_y', float('nan'))
        self.declare_parameter('max_z', float('nan'))

        self.declare_parameter('invert', False)

        self._last_raw: PointCloud2 | None = None
        self.add_on_set_parameters_callback(self._on_set_params)

        self.sub = self.create_subscription(
            PointCloud2, self.get_parameter('input_topic').value,
            self._on_cloud, 5)
        self.pub = self.create_publisher(
            PointCloud2, self.get_parameter('output_topic').value, 5)
        self.marker_pub = self.create_publisher(
            Marker, '/scan_box/marker', 5)

        self._last_stats = 0.0
        self._last_out: PointCloud2 | None = None
        self._timer = self.create_timer(1.0, self._on_timer)
        self.get_logger().info(
            f'scan_box_filter ready on '
            f'{self.get_parameter("input_topic").value} -> '
            f'{self.get_parameter("output_topic").value} '
            f'(frame {self.get_parameter("marker_frame").value})')

    # ------------------------------------------------------------------ box

    def _bounds(self):
        """Return (min, max) triples from explicit bounds or center+size."""
        p = self.get_parameter
        cx, cy, cz = p('center_x').value, p('center_y').value, p('center_z').value
        sx, sy, sz = p('size_x').value, p('size_y').value, p('size_z').value

        mins = [p('min_x').value, p('min_y').value, p('min_z').value]
        maxs = [p('max_x').value, p('max_y').value, p('max_z').value]

        lo = []
        hi = []
        centers = [cx, cy, cz]
        sizes = [sx, sy, sz]
        half = [0.5 * s for s in sizes]
        for i in range(3):
            lo.append(mins[i] if math.isfinite(mins[i]) else centers[i] - half[i])
            hi.append(maxs[i] if math.isfinite(maxs[i]) else centers[i] + half[i])
        return lo, hi

    def _box_marker(self):
        lo, hi = self._bounds()
        c = [(lo[i] + hi[i]) / 2.0 for i in range(3)]
        s = [hi[i] - lo[i] for i in range(3)]
        m = Marker()
        m.header.frame_id = self.get_parameter('marker_frame').value
        m.header.stamp = self.get_clock().now().to_msg()
        m.ns = 'scan_box'
        m.id = 0
        m.type = Marker.CUBE
        m.action = Marker.ADD
        m.pose.position.x = c[0]
        m.pose.position.y = c[1]
        m.pose.position.z = c[2]
        m.pose.orientation.w = 1.0
        m.scale.x = s[0] if s[0] > 0 else 0.001
        m.scale.y = s[1] if s[1] > 0 else 0.001
        m.scale.z = s[2] if s[2] > 0 else 0.001
        m.color.r = 0.3
        m.color.g = 1.0
        m.color.b = 0.3
        m.color.a = 0.35
        return m

    # ------------------------------------------------------------- filtering

    def _on_cloud(self, msg):
        self._last_raw = msg
        self._filter_and_publish(msg)

    def _on_set_params(self, params):
        if self._last_raw is not None:
            self._last_stats = 0.0  # force immediate stats print
            self._filter_and_publish(self._last_raw)
        return SetParametersResult(successful=True)

    def _filter_and_publish(self, msg):
        fields = {f.name: f for f in msg.fields}
        if 'x' not in fields or 'y' not in fields or 'z' not in fields:
            return
        # color field may also be named 'colors' (older rtabmap output)
        rgb_field = next((f for name, f in fields.items() if name in RGB_NAMES), None)

        step = msg.point_step
        n = msg.width * msg.height
        if n == 0:
            return

        raw = np.frombuffer(msg.data, dtype=np.uint8).reshape((n, step))

        def view_f32(name):
            f = fields[name]
            return raw[:, f.offset:f.offset + 4].copy().view('<f4').reshape(-1)

        xyz = np.empty((n, 3), dtype=np.float32)
        xyz[:, 0] = view_f32('x')
        xyz[:, 1] = view_f32('y')
        xyz[:, 2] = view_f32('z')

        invert = self.get_parameter('invert').value
        lo, hi = self._bounds()
        mask = np.ones(n, dtype=bool)
        for i in range(3):
            axis = xyz[:, i]
            mask &= (axis >= lo[i]) & (axis <= hi[i])
        if invert:
            mask = ~mask
            # drop NaN / far garbage when inverting
            for i in range(3):
                axis = xyz[:, i]
                mask &= np.isfinite(axis) & (np.abs(axis) < 1e4)
        keep = np.where(mask)[0]

        self._log_stats(n, int(mask.sum()))

        out = PointCloud2()
        out.header = msg.header
        out.height = 1
        out.width = keep.size
        out.is_dense = True
        out.point_step = 20
        out.row_step = 20 * keep.size
        fields_out = [
            PointField(name='x', offset=0, datatype=F32, count=1),
            PointField(name='y', offset=4, datatype=F32, count=1),
            PointField(name='z', offset=8, datatype=F32, count=1),
        ]
        if rgb_field is not None and keep.size > 0:
            rgb_bytes = raw[:, rgb_field.offset:rgb_field.offset + 4].copy()
            rgb = rgb_bytes.view('<u4').reshape(-1) if rgb_field.datatype == U32 \
                else rgb_bytes.view('<f4').reshape(-1).astype(np.uint32)
            fields_out.append(PointField(name='rgb', offset=12, datatype=U32, count=1))
            out.fields = fields_out
            packed = np.empty((keep.size, 4), dtype=np.float32)
            packed[:, 0:3] = xyz[keep]
            packed[:, 3] = rgb[keep].view(np.float32)
            out.data = packed.tobytes()
        else:
            out.fields = fields_out[:3]
            out.point_step = 12
            out.row_step = 12 * keep.size
            out.data = xyz[keep].tobytes()

        self.marker_pub.publish(self._box_marker())
        self.pub.publish(out)
        self._last_out = out

    def _on_timer(self):
        if self._last_out is not None:
            self._last_out.header.stamp = self.get_clock().now().to_msg()
            self.pub.publish(self._last_out)

    def _log_stats(self, total, in_box):
        now = self.get_clock().now().nanoseconds * 1e-9
        if now - self._last_stats >= 3.0 or self._last_stats == 0.0:
            self._last_stats = now
            lo, hi = self._bounds()
            self.get_logger().info(
                f'box filter: {in_box} / {total} points in cube '
                f'x[{lo[0]:.2f},{hi[0]:.2f}] y[{lo[1]:.2f},{hi[1]:.2f}] '
                f'z[{lo[2]:.2f},{hi[2]:.2f}]')


def main(args=None):
    rclpy.init(args=args)
    node = ScanBoxFilter()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()