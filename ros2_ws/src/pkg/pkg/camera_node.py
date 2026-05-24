import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
from rclpy.qos import qos_profile_sensor_data

import time
from std_msgs.msg import Bool

import cv2


class CameraNode(Node):

    def __init__(self):
        super().__init__('camera_node')

        self.publisher_ = self.create_publisher(
            Image,
            'camera/image',
            qos_profile_sensor_data
        )
        self.record_sub_ = self.create_subscription(
            Bool,
            '/record_video',
            self.record_callback,
            10
        )

        self.bridge = CvBridge()
        
        self.is_recording = False
        self.video_writer = None

        # Open camera ONLY ONCE
        self.cap = cv2.VideoCapture(0, cv2.CAP_V4L2)

        # Set resolution
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        self.cap.set(cv2.CAP_PROP_FPS, 30)

        # 30 FPS
        self.timer = self.create_timer(1.0 / 30.0, self.publish_image)

    def record_callback(self, msg):
        self.is_recording = msg.data
        if self.is_recording and self.video_writer is None:
            filename = 'drone_video_' + time.strftime("%Y%m%d_%H%M%S") + '.avi'
            width = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            fps = self.cap.get(cv2.CAP_PROP_FPS)
            if fps == 0 or fps != fps: # NaN or 0 check
                fps = 30.0
            fourcc = cv2.VideoWriter_fourcc(*'XVID')
            self.video_writer = cv2.VideoWriter(filename, fourcc, fps, (width, height))
            self.get_logger().info(f'Started recording video: {filename}')
        elif not self.is_recording and self.video_writer is not None:
            self.video_writer.release()
            self.video_writer = None
            self.get_logger().info('Stopped recording video.')

    def publish_image(self):

        ret, frame = self.cap.read()

        if not ret:
            self.get_logger().warning('Failed to capture image')
            return

        if self.is_recording and self.video_writer is not None:
            self.video_writer.write(frame)

        img_msg = self.bridge.cv2_to_imgmsg(frame, encoding='bgr8')

        self.publisher_.publish(img_msg)

        cv2.imshow('camera', frame)
        cv2.waitKey(1)

    def destroy_node(self):

        if self.video_writer is not None:
            self.video_writer.release()
        self.cap.release()
        cv2.destroyAllWindows()

        super().destroy_node()


def main(args=None):

    rclpy.init(args=args)

    camera_node = CameraNode()

    rclpy.spin(camera_node)

    camera_node.destroy_node()

    rclpy.shutdown()


if __name__ == '__main__':
    main()