import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import serial
import time

class ESP32ControlNode(Node):
    def __init__(self):
        super().__init__('esp32_control_node')

        # Configuration
        serial_port = self.declare_parameter('serial_port', '/dev/ttyUSB0').value
        baud_rate = self.declare_parameter('baud_rate', 115200).value

        # Initialize Serial Connection
        try:
            self.serial_conn = serial.Serial(serial_port, baud_rate, timeout=1)
            time.sleep(2)  # Wait for ESP32 to reset upon serial connection
            self.get_logger().info(f"Connected to ESP32 on {serial_port} at {baud_rate} baud.")
        except serial.SerialException as e:
            self.get_logger().error(f"Failed to connect to ESP32: {e}")
            self.serial_conn = None

        # Subscribe to /cmd_vel to get movement commands
        self.subscription = self.create_subscription(
            Twist,
            '/cmd_vel',
            self.cmd_vel_callback,
            10
        )

    def cmd_vel_callback(self, msg):
        if not self.serial_conn:
            self.get_logger().warn("Serial connection not active. Skipping command.")
            return

        # Map linear.x (-1.0 to 1.0) to Motor PWM (1000 to 2000)
        # Assuming 1500 is stop, >1500 is forward, <1500 is backward
        # Be careful to adjust the multiplier based on your desired max speed!
        speed_factor = msg.linear.x
        motor_pwm = int(1500 + (speed_factor * 500))
        motor_pwm = max(1000, min(2000, motor_pwm)) # Clamp

        # Map angular.z (-1.0 to 1.0) to Servo Angle (0 to 180)
        # Assuming 90 is center
        turn_factor = msg.angular.z
        servo_angle = int(90 + (turn_factor * 90))
        servo_angle = max(0, min(180, servo_angle)) # Clamp

        # Create command string matching ESP32 firmware parser
        command = f"M:{motor_pwm},S:{servo_angle}\n"
        
        try:
            self.serial_conn.write(command.encode('utf-8'))
            self.get_logger().debug(f"Sent to ESP32: {command.strip()}")
            
            # Read ACK if any (optional, can be removed to increase performance)
            if self.serial_conn.in_waiting > 0:
                ack = self.serial_conn.readline().decode('utf-8').strip()
                self.get_logger().debug(f"ESP32 replied: {ack}")
        except Exception as e:
            self.get_logger().error(f"Failed to write to Serial: {e}")

def main(args=None):
    rclpy.init(args=args)
    node = ESP32ControlNode()
    
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if node.serial_conn:
            node.serial_conn.close()
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
