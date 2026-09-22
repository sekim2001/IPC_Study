+ 둘 이상의 프로세스가 동일한 물리 메모리(RAM) 세그먼트를 각자의 가상 주소 공간(Virtual Address Space)에 매핑하여 직접 읽고 쓰는 가장 빠른 IPC 기법 (Zero-Copy)

## POSIX 공유 메모리와 `/dev/shm`

POSIX Shared Memory는 커널에 의해 메모리 기반의 가상 파일 시스템 (__tmpfs__)을 `/dev/shm` 에 파일 형태로 마운트되어 관리된다.

* 실제 디스크 I/O가 발생하지 않고 순수 RAM에서 동작
* `ls -l /dev/shm` 명령어로 현재 시스템에 생성된 공유 메모리 객체를 조회
* 프로세스가 종료되더라도 `shm_unlink()`를 명시적으로 호출하거나 시스템을 재부팅하기 전까지 커널에 공유 메모리는 남아있음

---

## POSIX 공유 메모리 핵심 API

+ POSIX 공유 메모리는 `<sys/mman.h>`, `<sys/stat.h>`, `<fcntl.h>` 헤더를 사용하며, 컴파일 시 `lrt` 링크 옵션이 필요

### 1. `shm_open()` - 공유 메모리 객체 생성/열기
```c
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

int shm_open(const char *name, int oflag, mode_t mode);
```

+ __`name`__: 공유 메모리 객체 이름 (반드시 `/이름` 형식의 슬래시로 시작해야 함)
+ __`oflag`__:
  + 접근 모드: `O_RDONLY` (읽기 전용), `O_RDWR` (읽기/쓰기)
  + 생성 플래그:
    + `O_CREAT`: 객체가 없으면 새로 생성 (`mode` 인자 필요)
    + `O_EXCL`: `O_CREAT`와 함께 사용하여 이미 존재할 경우 에러(`EEXIST`) 반환
    + `O_TRUNC`: 이미 존재할 경우 크기를 0으로 초기화
+ __`mode`__: 파일 접근 권한 (예: `0666`, `0644`)
+ __반환값__: 성공 시 파일 디스크립터(fd), 실패 시 `-1` (`errno` 설정)

### 2 `ftruncate()` - 공유 메모리 크기 설정 

+ Kernel Space에 실제 물리적 공간 (RAM/tmpgs)을 확보 

```c
#include <unistd.h>

int ftruncate(int fd, off_t length);
```
+ `shm_open()`으로 새로 생성된 공유 메모리 객체의 초기 크기는 __0바이트__ 
+ `ftruncate()`를 호출하여 실제 사용할 크기(바이트)만큼 메모리를 할당하지 않고 `mmap()`을 수행하면, 메모리 접근 시 __`SIGBUS` (Bus Error)__ 시그널이 발생하며 프로세스가 강제 종료됨

### 3 `mmap()` - 가상 메모리 공간 매핑

+ 커널에 확보된 물리공간을 프로세스의 가상 주소 공간으로 포인터 주소를 맴핑(바인딩) => User Space

```c
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```
+ __`addr`__: 매핑할 시작 주소 권장값 (`NULL` 지정 시 커널이 최적 주소 자동 배정)
+ __`length`__: 매핑할 바이트 크기
+ __`prot`__: 메모리 보호 플래그
  + `PROT_READ`: 읽기 허용
  + `PROT_WRITE`: 쓰기 허용
  + `PROT_EXEC`: 실행 허용
  + `PROT_NONE`: 접근 불가
+ __`flags`__:
  + `MAP_SHARED`: **다른 프로세스와 메모리를 공유**. 쓰기 작업이 실제 공유 객체에 즉시 반영됨 (IPC 필수)
  + `MAP_PRIVATE`: 프로세스 전용 매핑 (Copy-on-Write 동작, 변경 사항이 다른 프로세스에 보이지 않음)
+ **`fd`**: `shm_open()`으로 얻은 파일 디스크립터
* **`offset`**: 매핑 시작 오프셋 (페이지 크기 배수여야 함, 보통 `0`)
* **반환값**: 성공 시 매핑된 메모리 포인터, 실패 시 `MAP_FAILED` (`(void *)-1`)

### 4 `munmap()` & `close()` - 매핑 해제 및 FD 닫기
```c
int munmap(void *addr, size_t length);
int close(int fd);
```
* `munmap()`: 현재 프로세스의 가상 주소 공간에서 매핑을 제거
* `close()`: 공유 메모리 파일 디스크립터를 인자로 사용 (매핑이 유지되는 한 `close()` 후에도 메모리 접근은 가능하지만, 명시적으로 닫아주는 것이 안전)

### 5 `shm_unlink()` - 공유 메모리 객체 제거
```c
int shm_unlink(const char *name);
```
* 시스템(`/dev/shm`)에서 해당 이름의 공유 메모리 객체를 삭제
* 파일의 `unlink()`와 동일하게 **참조 카운트(Reference Count)** 기반으로 동작.
* 이미 매핑 중인 프로세스는 계속 사용할 수 있으며, 모든 프로세스가 `munmap()`/`close()`를 마치면 물리 메모리에서 완전 소멸함

---

## 동기화(Synchronization)

### 1 동기화가 필요한 이유
* **커널 자동 제어의 부재**
+ 파이프나 메시지 큐는 커널이 내부적으로 버퍼와 락을 제어하여 송수신 시 자동으로 블로킹/동기화를 처리
* 공유 메모리는 커널 개입 없이 사용자 메모리에 직접 읽고 쓰기 때문에 **경쟁 상태(Race Condition)**가 필연적으로 발생
  * Reader가 데이터를 읽고 있는 도중에 Writer가 새 데이터를 덮어써 데이터가 손상(Data Corruption)되는 문제
  * Writer가 데이터를 아직 다 쓰지 않았는데 Reader가 불완전한 데이터를 읽어가는 문제

### 2 임계 영역(Critical Section) 보호
공유 메모리를 사용할 때는 반드시 상호 배제(Mutual Exclusion) 또는 순서 동기화(Signaling) 메커니즘을 함께 설계해야 함

---

## 동기화 기법 비교: POSIX Semaphore vs Mutex

| 구분 | POSIX Semaphore (세마포어) | POSIX Mutex (뮤텍스) |
| :---: | :---: | :---: |
| **개념** | 신호(Signal) 기반 / 수량 카운팅 메커니즘 | 소유권(Ownership) 기반 상호 배제 락 |
| **소유권** | **없음** (A 프로세스가 `wait`하고 B 프로세스가 `post` 가능) | **있음** (Lock을 획득한 쓰레드/프로세스만 Unlock 가능) |
| **주요 목적** | 프로세스 간 실행 순서 동기화, 생산자-소비자 신호 전달, 자원 개수 관리 | 임계 영역(Critical Section) 독점 보호 |
| **초기값/상태** | 0 이상의 정수 (이진 세마포어: 0 or 1, 카운팅 세마포어: N) | Locked(1) or Unlocked(0) |
| **우선순위 역전 대응** | 기본 미지원 (신호 전달용이므로 소유자 개념 없음) | `PTHREAD_PRIO_INHERIT` 등 우선순위 상속 지원 |
| **공유 메모리 활용** | 프로세스 간 동기화에 가장 널리 사용 (`pshared = 1`) | `PTHREAD_PROCESS_SHARED` 속성 지정 시 사용 가능 |

### 1 Unnamed Semaphore를 통한 프로세스 간 동기화

공유 메모리 구조체 내부에 `sem_t`를 배치하고 `sem_init()` 시 `pshared = 1`로 설정하면 별도의 커널 객체 파일 없이 메모리 영역 내에서 직접 프로세스 간 동기화가 가능

```c
typedef struct {
    sem_t sem_sync;                              // 동기화 세마포어
    char  data[1024];                            // 공유 데이터
} shm_data_t;

// pshared = 1 (프로세스 간 공유), value = 0 (초기값 0)
sem_init(&shm_ptr->sem_sync, 1, 0);
```

### 2 pthread_mutex를 공유 메모리에 배치하는 방법
뮤텍스도 공유 메모리에 두고 프로세스 간 상호 배제에 활용할 수 있다. 단, 반드시 `pthread_mutexattr_t` 속성에서 **`PTHREAD_PROCESS_SHARED`**를 설정해야 합니다.

```c
typedef struct {
    pthread_mutex_t mutex;
    int counter;
} shm_mutex_data_t;

pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); // 프로세스 간 공유 속성 지정
pthread_mutex_init(&shm_ptr->mutex, &attr);
pthread_mutexattr_destroy(&attr);
```

---

## POSIX Semaphore의 종류

| 구분 | Unnamed Semaphore (무명 세마포어) | Named Semaphore (명명 세마포어) |
| :---: | :---: | :---: |
| **생성/초기화** | `sem_init(&sem, pshared, value)` | `sem_open(name, O_CREAT, mode, value)` |
| **저장 위치** | 사용자가 할당한 메모리 영역 (공유 메모리 또는 전역 변수) | `/dev/shm/sem.<name>` (커널 가상 파일) |
| **식별 방식** | 메모리 주소 포인터 (`sem_t *`) | 문자열 경로/이름 (`"/sem_test"`) |
| **적합한 관계** | 부모-자식 프로세스(`fork()`) 또는 동일 SHM을 매핑한 프로세스 | 부모-자식 관계가 없는 완전히 독립된 프로세스 간 |
| **해제 함수** | `sem_destroy(&sem)` | `sem_close(sem)` + `sem_unlink(name)` |

| 구분 | Unnamed Semaphore (무명 세마포어) | Named Semaphore (명명 세마포어) |
| :---: | :---: | :---: |
| __카운터 범위__ | 0 또는 1 | 0 이상의 정수 N |
| __주요 목적__ | 상호 배제 -> 하나의 프로세스/스레드의 접근 허용 | 자원 관리 -> 동시에 N개의 프로세스/스레드 접근 허용 |

### 세마포어 카운터 
+ __카운터 > 0__ : 사용 가능한 자원이 남아 있어 프로세스가 대기(wait) 없이 즉시 진입하여 자원 사용 가능
+ __카운터 == 0__ : 모든 자원이 다른 프로세스들에 의해 사용 중임을 의미, 진입하려는 프로세스는 대기(blocking)

### 주요 세마포어 함수
* `sem_wait(sem_t *sem)`: 세마포어 값이 0보다 크면 1 감소시키고 즉시 반환, 0이면 1 이상이 될 때까지 **블로킹(대기)**
* `sem_trywait(sem_t *sem)`: 블로킹되지 않고 즉시 반환, 세마포어가 0이면 `-1` 및 `errno = EAGAIN` 반환
* `sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)`: 지정된 **절대 시각(Absolute Time)**까지 대기 후 타임아웃(`ETIMEDOUT`)
* `sem_post(sem_t *sem)`: 세마포어 값을 1 증가시키고 대기 중인 프로세스/쓰레드를 깨움
* `sem_getvalue(sem_t *sem, int *sval)`: 현재 세마포어의 정수값 조회

---

## 공유 메모리 설계 규칙 및 주의사항

### 1. 구조체 내부에 포인터(Pointer) 절대 사용 금지
* 각 프로세스는 독립적인 **가상 메모리 주소 공간(Virtual Address Space)**을 갖습니다.
* 동일한 공유 메모리 객체를 매핑하더라도, OS가 배정한 가상 시작 주소(`shm_ptr`)는 프로세스마다 다를 수 있습니다.
* 포인터 변수를 공유 메모리에 쓰면 다른 프로세스가 역참조할 때 **`Segmentation Fault`**가 발생합니다.
* **해결책**: 고정 크기 배열을 사용하거나, 상대적 거리인 **오프셋(Offset)** 방식으로 데이터를 가리켜야 합니다.

### 2. `ftruncate()` 누락 주의
* 새로 생성된 공유 메모리는 크기가 0입니다. `ftruncate()`를 호출해 명시적으로 크기를 늘리지 않고 메모리에 접근하면 **`SIGBUS` (Bus Error)**가 발생합니다.

### 3. `shm_open()` 전/후의 `shm_unlink()` 정리
* 프로그램이 비정상 종료(`Ctrl+C`, `SIGSEGV`)되면 이전 실행의 공유 메모리가 `/dev/shm`에 그대로 남아 있습니다.
* 새로 시작할 때 기존 쓰레기 데이터나 잘못된 세마포어 상태가 로드되는 것을 방지하기 위해, **초기화 시점에 `shm_unlink()`를 먼저 호출**하거나 안전한 종료 루틴(Signal Handler)에서 `shm_unlink()`를 보장해야 합니다.

### 4. 구조체 패딩(Padding)과 메모리 정렬(Alignment)
* 32비트/64비트 아키텍처 차이 또는 컴파일러 최적화 옵션에 따라 구조체 멤버 간 패딩 바이트가 달라질 수 있습니다.
* 송신 프로세스와 수신 프로세스가 **동일한 헤더 파일의 구조체 정의를 공유**해야 데이터 왜곡을 방지할 수 있습니다.

--- 
## snprintf()

+ Buffer Overflow를 방지하면서 문자열을 안전하게 Formatting하여 출력할때 사용하는 함수

```C
#include <stdio.h>

int snprintf(char *str, size_t size, const char *format, ...);
```
+ __str__ : 생성된 문자열이 저장될 버퍼 메모리의 시작 주소 포인터
+ __size__ : str 버퍼에 기록할 최대 바이트 수 (\0 포함)
+ __format__ : 형식 지정자 (%d, %s, %x 등)를 포함한 서식 문자열
+ __...__ : 형식 지정자에 대응하는 가변 인자 


---
+  __세마포어 연산 권한__
  + sem_post()는 세모포어 내부 변수(상태 값)를 읽고 -> +1 증가 시키는(write) 과정의 연산을 수행
  + mmap()시 권한을 __PROT_READ | PROT_WRITE__ 로 설정하지 않고 sem_post 시 세그멘테이션 오류(SIGSEGV)나 권한 오류가 발생

+ 세마포어 중복 초기화
  + 송신자/수신자 양쪽에서 sem_init() 호출하면 __대기 중이던 세마포어의 내부 상태가 리셋되어 오동작한다.__
  + 공유 메모리를 생성(O_CREAT)하는 생성자에서 딱 한본  sem_init() 호출

+ __volatile__ 키워드
  + volatile 키워드로 선언된 변수는 최적화하지 않고 실제 매모리 주소에서 직접 읽고 쓰도록 컴파일러에게 지시
  + __예시__
    + 폴링 플래그 최적화
    ```c
    int ready = 0;
  
    while (!ready) {
        // 다른 프로세스가 ready를 1로 바꿔주길 대기
    }
    ```
  &ensp;&ensp;&ensp; 일반 변수 (int ready;) : <br>
  &ensp;&ensp;&ensp;  - 컴파일러는 루드 내부에 `ready`를 변경하는 코드가 없어 메모리를 매변 확인하디 않고 레지스터에 저장된 0만 확인 <br>
  &ensp;&ensp;&ensp;  - 코드가 내부적으로 `while(true);`와 같은 무한 루프로 궅어져, 다른 프로세스가 공유 메모리가 변경해도 인지하디 못한다.
  &ensp;&ensp;&ensp; volatile 변수 (volatile int ready;):<br>
  &ensp;&ensp;&ensp;  - 컴파일러는 매 반복마다 RAM(동유 메모리) 주소에서 최신 값을 읽어온다 

    + 폴링 플래그 최적화
    ```c
    for (int i = 0; i < 100000; i++) {
      shm_ptr->counter++;
    }
    ```
  &ensp;&ensp;&ensp; 일반 변수 (int counter;) : <br>
  &ensp;&ensp;&ensp;  - 컴파일러는 counter를 사용하는 다른 코드가 없어 counter += 100000 후 메모리에 저장한다.<br>

  + volatile이 사용되는 경우
    + IPC 공유 메모리 및 멀티 쓰레드 공유 플래그
    + 임메지즈 메모리 맵 I/O (MMIUO)
    + 시그널 핸들러 / 인터럽트 서비스 루틴 (ISR) 


---
## 부모-자식 프로세스의 wait(NULL) 동작
+ 자식 프로세스 1개가 끝날때까지 부모 프로세스를 Blocking하는 1회용 함수과제 2

+ for (int i = 0; i < 4; i++) wait(NULL);
```C
  i = 0 일 때:
    1. wait(NULL) 첫번째 호출
    2. 부모 프로세스 일시 정지 (Sleep / Block)
    3. [자식 1]이 exit() 종료
    4. 커널이 부모를 깨우고 자식 1의 자원을 정리(Reap)한 뒤 wait()가 반환

  i = 1 일 때:
    5. wait(NULL) 두 번째 호출!
    6. 부모 프로세스 일시 정지 (Sleep / Block)
    7. [자식 2]가 exit() 종료
    8. 커널이 부모를 깨우고 wait() 반환.

  i = 2 일 때:
    9. 세 번째 wait(NULL) 호출 및 정지 → [자식 3] 종료 시 깨어남.

  i = 3 일 때:
    10. 네 번째 wait(NULL) 호출 및 정지 → [자식 4] 종료 시 깨어남.
```

+ 자식 프로세스가 종료될 때 부모 프로세스가 wait()를 호출하지 않으면, 자식 프로세스는 좀비상태로 커널의 종료 큐에 대기한다
  + wait()는 좀비 상태의 자식프로세스를 수거하고 즉시 반환한다

