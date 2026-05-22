import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
from rclpy.qos import qos_profile_sensor_data

import cv2


class CameraNode(Node):

    def __init__(self):
        super().__init__('camera_node')

        self.publisher_ = self.create_publisher(
            Image,
            'camera/image',
            qos_profile_sensor_data
        )

        self.bridge = CvBridge()

        # Open camera ONLY ONCE
        self.cap = cv2.VideoCapture(0, cv2.CAP_V4L2)

        # Set resolution
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        self.cap.set(cv2.CAP_PROP_FPS, 30)

        # 30 FPS
        self.timer = self.create_timer(1.0 / 30.0, self.publish_image)

    def publish_image(self):

        ret, frame = self.cap.read()

        if not ret:
            self.get_logger().warning('Failed to capture image')
            return

        img_msg = self.bridge.cv2_to_imgmsg(frame, encoding='bgr8')

        self.publisher_.publish(img_msg)

        cv2.imshow('camera', frame)
        cv2.waitKey(1)

    def destroy_node(self):

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