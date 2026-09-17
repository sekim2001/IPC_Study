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

+ 비블로킹 모드
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
    