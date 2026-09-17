__메시지 큐 속정 정의 구조체__
```
struct mq_attr
{
  __syscall_slong_t mq_flags;	// 메시지 큐 플래그 (블로킹/비블로킹)
  __syscall_slong_t mq_maxmsg;	// 큐에 들어갈 수 있는 최대 메시지 수 
  __syscall_slong_t mq_msgsize;	// 메시지 1개당 최대 바이크 크기
  __syscall_slong_t mq_curmsgs;	// 현재 큐에 대기 중인 메시지 개수
  __syscall_slong_t __pad[4];
};
```
+ 메세지 큐 플래그에 따른 동작 차이
| 상황 | 0 (블로킹 모드) | O_NONBLOCK (비블로킹)|
| :---: | :---: | :---: |
| 큐가 비었을때 mq_receive() 호출 | 메시지가 들어올 때까지 대기 | 즉시 반환 (실패 처리), errno에 EAGAIN 설정 |
| 큐가 꽉 찼을때 mq_send() 호출 | 비자리가 생길 때까지 대기 | 즉시 반환 (실패 처리), errno에 EAGAIN 설정 |

+ 블로킹 모드
    + __장점__ : 대기 중 CPU 사용률은 0%이고, 메시지 수신 시 커널이 프로세스를 깨운다
    + __단점__ : 대기 중 다른 작업 진행 불가
    + __사용__ : 코드가 단순하고 직관적이며, 오직 메시지를 처리하는 전용 작업자 스레드/프로세스일 때

+ 비블로킹 모드a
    + __장점__ : 큐에 메시지가 없어도 다른 작업 수행 가능
    + __단점__ : 데이터를 읽으려 무한 루프(while(1) {mq_receive(...); })를 돌리면, 빈큐를 조회하느라 CPU사용률이 치솟을 수 있음.
    + __사용__ : 백그라운드 로직을 계속 수행해야 할 때 (epoll, select 또는 mq_notify 와 함께 사용)

---
__mq_open() 함수 인자__
+ __mqd_t mq_open(const char *name, int oflag);__ // 큐를 기존에 생성된 것을 열기만 할 때
+ __mqd_t mq_open(const char *name, int oflag, mode_t mode, struct mq_attr *attr);__ // O_CREAT 플래그를 넣어 새로 생성할 때 (가변 인자 형태)

+ int oflag (열기/생성 동작 플래그) 
    + 접근 모드 (반드시 3개 중 1개 선택):
        + O_RDONLY: 수신(mq_receive) 전용
        + O_WRONLY: 송신(mq_send) 전용
        + O_RDWR: 송수신 모두 가능
    + 생성 및 제어 플래그 (선택 조합):
        + O_CREAT: 큐가 없으면 새로 생성합니다. (이 플래그를 쓰면 뒤의 3, 4번째 인자인 mode, attr가 필요)
        + O_EXCL: O_CREAT와 함께 사용 시, **이미 같은 이름의 큐가 존재하면 실패(EEXIST)**를 반환합니다. (중복 생성 방지 및 원자적 생성 보장)
        + O_NONBLOCK: 비블로킹(Non-blocking) 모드로 엽니다. 큐가 꽉 찼을 때 mq_send를 하거나 비었을 때 mq_receive를 하면 멈추지 않고 즉시 -1과 함께 errno = EAGAIN을 반환합니다.

+ mode_t mode (접근 권한) - O_CREAT 지정 시 필수
    + 파일 권한(Permission)과 동일하게 8진수나 매크로로 지정
    + 예: 0666 (모든 사용자가 읽기/쓰기 가능), 0644 (소유자 읽기/쓰기, 그룹/기타 읽기 전용)

+ struct mq_attr *attr (큐 속성) — O_CREAT 지정 시 사용
    + struct mq_attr 구조체 포인터로, 큐의 용량(mq_maxmsg, mq_msgsize)을 지정
    + __NULL 전달시__ 커널의 기본 설정값 (/proc/sys/fs/mqueue/ 내부 기본값)으로 자동 생성


---
__Message Queue 설계 규칙__
### 1. 메시지 구조체 안에 포인터 포함 불가
+ IPC는 서로 다른 가상 메모리 주소 공간(Virtual Address Space)을 가진 프로세스 간 통신
+ 구조체에 포인트 사용 시 Segmentation Fault 발생 => 고정 크기 버퍼 사용

### 2. 구조체 크기 <= mq_msgsize 
+ mq_open()시 지정한 mq_msgsize 보다 큰 데이터를 mq_send()할 때 EMSGIZE 에러 발생
+ __TIP__ : mq_attr .mq_msgsize에 크기를 하드코딩 하기 보다는 sizeof(Packet)을 기준으로 설정

### 3. 송신자와 수신자는 동일한 헤더 파일 공유
+ 구조체 멤버의 순서나 자료형이 송신자와 수신자가 다르면 데이터가 깨지는 현상 발생
+ 송신자와 수신자에서 구조체를 선언하지 말고, 공통 헤더 파일에 구조체를 정의한다.


---
__Pipe 와 Message Queue 의 차이__
+ 파이프는 송신 프로세스가 닫으면 수신 측 read()가 0(EOF)을 반환
+ POSIX 메시지 큐는 커널이 관리하는 독립 IPC 객체이므로, 송신자가 닫고 종료되어도 큐는 살아있음
+ mq_unlink(), mq_close()실행 되지 않으면 /dev/mqueue에 메시지큐가 남아 있다


---
__mq_open() 하기 전에 mq_unlink() 부터 호출하는 이유__
+ 프로세스의 강제 종료 사 메시지는 커널(/dev/mqueue)에 남아 있음
    + 의도치 않은 쓰레기 데이터를 읽을 수 있음
    + O_CREAT => '큐가 없으면 만들고, 있으면 그냥 연다' => 큐의 속성(Attribute) 설정이 무시되고 기존 규격의 큐가 열림
+ 큐가 없을 시 mq_unlink() 호출 시 실패하여 -1을 반환, errno에는 ENOENT(No such file or directory)가 설정됨

---
__mq_close()와 mq_unlink()의 차이__
| 구분 | mq_close(mqd) | mq_unlink(namw) |
| :---: | :---: | :---: |
| 대상 | 프로세스의 디스크럽터 | 시스템/커널의 큐 이름 |
| 의미 | 프로세스가 큐의 사용을 끝냄 | 시스템에서 큐의 이름을 지우고 파괴 |
| 큐 | 큐와 메시지는 커널에 그대로 유지됨 | 참조 카운트가 0이 되는 순간 완전 소멸 |
| 프로세스 | 다른 프로세스는 여전히 읽고 쓰기 가능 | 다른 프로세스에서 open 불가 |


--- 
__Message Queue TimeOut__
+ Message Queue가 살아 있으고 메시지가 없으면 mq_receive()은 무한 대기 상태 => __timeout__ 설정
+ mq_timedreceive() 및 POSIX 조건 변수, 세마포어 등은 "XX초 뒤" 라는 상대시간(Relative Time)가 아닌 "xxx년 xx월 xx일 xx시 xx분 xx초 까지" 라는 __절대적인 마감 시각(Absolute Beadline)__ 요구
    + [상태 시간 방식] : (2초 대기) --> (2초 카운트다운) --> (종료)
    + [POSIX 절대 시간 방식] : (현재 10:00:00) -->  (목표 10:00:02 설정) --> (10:00:02 시각에 타임아웃 발생)
+ 타임 아웃 메시지 큐 동작 흐름 : 시각 조회 -> 목표 시각 연산 -> mq_timefreceive() 호출
    + 목표 시각 안에 메시지 __수신__ : 프로세스가 깨어나 데어터 버퍼에 메시지 복사 (반환값 : 읽은 바이트 수 >= 0)
    + 목표 시각 안에 메시지 __미수신__ : 커널 타이머 인터럽트 발생 (반환값: -1 / errno : ETIMEDOUT 설정)
&ensp;&ensp;&ensp; 1. clock_gettime(CLOCK_REALTIME, &timeout);<br>
&ensp;&ensp;&ensp;&ensp; - CLOCK_REALTIME은 리눅스 시스템의 실제 시간(WALL-clock time) 의미<br>
&ensp;&ensp;&ensp;&ensp; - 함수 호출 시점의 시스템 현재 시각을 초(tv_sec)와 나노초(tv_nesc) 단위로 저장<br>
&ensp;&ensp;&ensp; 2. timeout.tv_sec += TIMEOUT;<br>
&ensp;&ensp;&ensp;&ensp; - 현재 시각의 초단위 시간을 더해 타임아웃으로 설정할 목표 절대 시각 설정<br>
&ensp;&ensp;&ensp; 3. mq_timedreceive(..., &timeout);<br>
&ensp;&ensp;&ensp;&ensp; - 요청 프로세스는 커널에 큐 읽기를 요청하며 대기(Sleep/Block)에 돌입<br>
&ensp;&ensp;&ensp;&ensp; - 메시지가 도착 시 타임아웃 시각에 도달하지 않아도 프로세스가 깨어나 메시지를 반환<br>
&ensp;&ensp;&ensp;&ensp; - 타임아웃 시각 초과 시 커널이 프로세스를 깨우고 -1을 반환<br>
