import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import sys
import select
import termios
import tty

# Phím mũi tên (Arrow keys) sẽ trả về chuỗi escape sequence bắt đầu bằng \x1b
settings = termios.tcgetattr(sys.stdin)

msg = """
Điều khiển xe bằng phím mũi tên!
---------------------------
Mũi tên Lên   : Tiến tới
Mũi tên Xuống : Lùi lại
Mũi tên Trái  : Đánh lái trái
Mũi tên Phải  : Đánh lái phải
Phím 'Space'  : Phanh khẩn cấp / Dừng lại
Bấm 'CTRL-C' để thoát
"""

def getKey():
    tty.setraw(sys.stdin.fileno())
    # Chờ phím bấm với timeout là 0.1s
    rlist, _, _ = select.select([sys.stdin], [], [], 0.1)
    if rlist:
        key = sys.stdin.read(1)
        # Nếu là phím Escape, đọc thêm 2 ký tự nữa để bắt phím mũi tên
        if key == '\x1b':
            key += sys.stdin.read(2)
    else:
        key = ''
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
    return key

class KeyboardControlNode(Node):
    def __init__(self):
        super().__init__('keyboard_control_node')
        self.publisher_ = self.create_publisher(Twist, '/cmd_vel', 10)
        
        self.speed = 0.5  # Tốc độ tiến/lùi mặc định
        self.turn = 0.5   # Góc đánh lái mặc định
        
        self.linear_x = 0.0
        self.angular_z = 0.0

        self.get_logger().info(msg)

    def run(self):
        try:
            while rclpy.ok():
                key = getKey()
                
                # Nếu không bấm gì, xe tự dừng (có thể tuỳ chỉnh nếu muốn xe tự giữ ga)
                # Tạm thời để chế độ: Bấm giữ thì chạy, thả ra thì dừng
                if key == '\x1b[A': # Mũi tên Lên
                    self.linear_x = self.speed
                    self.angular_z = 0.0
                elif key == '\x1b[B': # Mũi tên Xuống
                    self.linear_x = -self.speed
                    self.angular_z = 0.0
                elif key == '\x1b[D': # Mũi tên Trái
                    self.linear_x = self.speed
                    self.angular_z = self.turn
                elif key == '\x1b[C': # Mũi tên Phải
                    self.linear_x = self.speed
                    self.angular_z = -self.turn
                elif key == ' ': # Phím Space (Dừng)
                    self.linear_x = 0.0
                    self.angular_z = 0.0
                elif key == '\x03': # Ctrl+C
                    break
                else:
                    self.linear_x = 0.0
                    self.angular_z = 0.0

                twist = Twist()
                twist.linear.x = self.linear_x
                twist.angular.z = self.angular_z
                
                # Chỉ Publish khi có thay đổi (tránh spam quá nhiều nếu không bấm gì)
                # Hoặc cứ publish liên tục để đảm bảo xe nhận tín hiệu an toàn.
                self.publisher_.publish(twist)

        except Exception as e:
            print(e)

        finally:
            # Gửi lệnh dừng lại trước khi tắt
            twist = Twist()
            twist.linear.x = 0.0
            twist.angular.z = 0.0
            self.publisher_.publish(twist)
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)


def main(args=None):
    rclpy.init(args=args)
    node = KeyboardControlNode()
    node.run()
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
