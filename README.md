Hướng Dẫn Build ( Windows)
Tài liệu này mô tả các bước cần thiết để build ransomware C++ by yuri08 Windows. bao gồm việc biên dịch thư viện OpenSSL từ mã nguồn và sau đó liên kết nó với dự án C++ chính.

Điều Kiện 
Cần cài đặt các phần mềm sau:

Visual Studio 2019/2022:
Tải xuống phiên bản Community (miễn phí) từ trang web chính thức của Visual Studio.
Trong quá trình cài đặt, bắt buộc phải chọn workload "Desktop development with C++".
Perl:
OpenSSL sử dụng các kịch bản cấu hình bằng Perl.
Tải xuống và cài đặt ActivePerl hoặc StrawberryPerl.
NASM (Netwide Assembler):
Trình hợp dịch ngôn ngữ assembly, cần thiết để biên dịch một số phần của OpenSSL.
Tải xuống từ trang web chính thức của NASM và cài đặt. Đảm bảo thêm đường dẫn đến nasm.exe vào biến môi trường PATH của hệ thống.
Bước 1: Tải Về và Chuẩn Bị Mã Nguồn
1.1. Tải Mã Nguồn OpenSSL
Truy cập trang web chính thức của OpenSSL: https://www.openssl.org/source/
Tải xuống phiên bản nguồn ổn định mới nhất (ví dụ: openssl-3.0.13.tar.gz).
Giải nén tệp đã tải vào một thư mục có đường dẫn ngắn và không chứa khoảng trắng. Ví dụ: C:\openssl-3.0.13.
1.2. Chuẩn Bị Mã Nguồn Payload
Tạo một thư mục mới cho dự án của bạn, ví dụ: C:\RansomwareProject.
Tạo một tệp văn bản mới trong thư mục này, đặt tên là payload.cpp.
Sao chép toàn bộ mã nguồn C++ của payload vào tệp payload.cpp.
Bước 2: Biên Dịch Thư Viện OpenSSL
Đây là bước quan trọng nhất. Bạn phải thực hiện các lệnh sau trong Developer Command Prompt for VS. Môi trường này đã được cấu hình sẵn các biến môi trường cho Visual Studio.

Cách mở Developer Command Prompt:
Nhấn nút Start, gõ Developer Command Prompt và chọn phiên bản tương ứng với Visual Studio của bạn (ví dụ: Developer Command Prompt for VS 2022).

2.1. Điều Hướng và Cấu Hình
Trong cửa sổ Command Prompt, điều hướng đến thư mục OpenSSL đã giải nén và chạy lệnh cấu hình.

Để build cho 64-bit :
```
cd C:\openssl-3.0.13
perl Configure VC-WIN64A --prefix=C:\openssl-x64 --openssldir=C:\openssl-x64\ssl
```

Để build  32-bit (x86):
```

cd C:\openssl-3.0.13
perl Configure VC-WIN32 --prefix=C:\openssl-x86 --openssldir=C:\openssl-x86\ssl
```
2.2.  (Build)
Sau khi cấu hình thành công, chạy lệnh nmake để bắt đầu quá trình biên dịch. Quá trình này có thể mất vài phút.
```

nmake
```
2.3. Kiểm Tra 
Để đảm bảo các thư viện được build chính xác,  có thể chạy bộ kiểm thử.
```

nmake test
```

2.4. Cài Đặt
Cuối cùng, chạy lệnh nmake install để sao chép các file thư viện (.lib) và file header (.h) vào thư mục bạn đã chỉ định trong --prefix.
```
nmake install
```

Sau khi hoàn tất, sẽ tìm thấy các file cần thiết tại:
```
File header: C:\openssl-x64\include\openssl
File thư viện: C:\openssl-x64\lib
```
Bước 3: Tạo và Cấu Hình Dự Án Visual Studio
Đây là bước  để liên kết mã với thư viện OpenSSL.
Trong Solution Explorer (thường ở bên phải), nhấp chuột phải vào tên dự án (Cerberus) và chọn Properties.
Thiết lập Configuration và Platform:
Ở trên cùng của cửa sổ Properties, đảm bảo bạn đang chọn:
Configuration: Release (Để build bản tối ưu, không có thông tin gỡ lỗi).
Platform: x64 (Phải khớp với phiên bản OpenSSL đã build ở Bước 2).
Cấu hình C/C++:
Trong cây bên trái, đi đến C/C++ -> General.
Tìm đến thuộc tính Additional Include Directories.
Nhấp vào mũi tên thả xuống và chọn <Edit...>.
Thêm một dòng mới và dán đường dẫn đến thư mục include của OpenSSL:
```
C:\openssl-x64\include
``
Nhấn OK.
Cấu hình Linker:
Bước 4.1: Thư mục Library:
Trong cây bên trái, đi đến Linker -> General.
Tìm đến thuộc tính Additional Library Directories.
Nhấp vào mũi tên thả xuống và chọn <Edit...>.
Thêm một dòng mới và dán đường dẫn đến thư mục lib của OpenSSL:
```
C:\openssl-x64\lib
``
Nhấn OK.
Bước 4.2: Các file Library:
Trong cây bên trái, đi đến Linker -> Input.
Tìm đến thuộc tính Additional Dependencies.
Nhấp vào mũi tên thả xuống và chọn <Edit...>.
Một danh sách các thư viện sẽ hiện ra. Thêm các thư viện sau vào danh sách (mỗi thư viện trên một dòng):
```
libcrypto.lib
libssl.lib
advapi32.lib
shell32.lib
shlwapi.lib
user32.lib
gdi32.lib
wsock32.lib
ws2_32.lib
crypt32.lib
psapi.lib
ole32.lib
oleaut32.lib
```
Nhấn OK.
Nhấn Apply và sau đó là OK để đóng cửa sổ Properties.
Bước 4: Build Dự Án
Bây giờ mọi thứ đã sẵn sàng để build.
Trong menu trên cùng của Visual Studio, chọn Build -> Build Solution (hoặc nhấn phím tắt F7).
Visual Studio sẽ bắt đầu quá trình biên dịch.  có thể thấy tiến trình trong cửa sổ Output ở dưới cùng.
Lưu Ý Quan Trọng
Kiến trúc: Luôn đảm bảo rằng kiến trúc build của dự án Visual Studio (x64 hoặc x86) khớp với kiến trúc bạn đã build cho OpenSSL (VC-WIN64A hoặc VC-WIN32). Sự không khớp sẽ gây ra lỗi liên kết.
Đường dẫn: Nếu bạn đã cài đặt OpenSSL vào một thư mục khác C:\openssl-x64, hãy nhớ cập nhật các đường dẫn trong bước cấu hình Visual Studio cho chính xác.
