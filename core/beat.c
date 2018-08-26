#define LNLYLIB_API_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <float.h>
#include "core/beat.h"
#include "core/lapi.h"
#include "osi/base.h"

#define L_THREAD_MASTER 1000
#define L_THREAD_OTHERS 2001
#define L_THREAD_WORKER 3001

#define L_SERVICE_ALIVE             0x0001
#define L_DOCKED_SERVICE_IN_TEMPQ   0x0002
#define L_SRVC_FLAG_SERVICE_STOP    0x0004
#define L_SRVC_FLAG_SERVICE_DESTROY 0x0008
#define L_SRVC_FLAG_SERVICE_RESTART 0x0010
#define L_SRVC_FLAG_SOCK_LISTEN     0x0020
#define L_SRVC_FLAG_SOCK_CONNECT    0x0040
#define L_SRVC_FLAG_SOCK_PENDING    0x0080

#define L_MSSG_FLAG_DONT_AUTO_FREE 0x01
#define L_MSSG_FLAG_MOVE_MSSG_DATA 0x02
#define L_MSSG_FLAG_FREE_MSSG_DATA 0x04
#define L_MSSG_FLAG_IS_REMOTE_MSSG 0x08
#define L_WORK_FLAG_PROGRAM_EXIT 0x01

#define L_SRVC_MIN_TABLE_SIZE 512
#define L_SRVC_MIN_USER_SVID 256
#define L_SERVICE_BOOTER 0x01
#define L_SERVICE_KILLER 0x02
#define L_SERVICE_HARBOR 0x03
#define L_SRVC_MAX_CORE_SVID 0x0f

#define L_MSG_MIN_USER_MSG_ID     0xff
#define L_MSG_SERVICE_CREATE_REQ  0x01
#define L_MSG_SERVICE_ON_CREATE   0x02
#define L_MSG_SERVICE_STOP_REQ    0x03
#define L_MSG_SERVICE_ON_DESTROY  0x04
#define L_MSG_SERVICE_ROUND_FIN   0x05
#define L_MSG_PROGRAM_EXIT_REQ    0x06
#define L_MSG_PROGRAM_EXIT_RSP    0x07
#define L_MSG_SUBSRVC_CREATE_RSP  (L_CORE_MSG_GR + 0x0A)
#define L_MSG_SERVICE_EXIT_REQ    (L_CORE_MSG_GR + 0x0B)
#define L_MSG_SERVICE_RESTART_REQ (L_CORE_MSG_GR + 0x0C)

#define L_TIMER_MIN_TABLE_SIZE 64
#define L_TIMER_CANCELED_FLAG (0x80000000UL << 32)
#define L_TIMER_NOT_ACTIVE_ID 0x0
#define L_TIMER_ADD_FAILED_ID 0xe
#define L_TIMER_IMMED_FIRED_ID 0x0f
#define L_TIMER_MIN_VALID_TMID 0x0f

#define L_MSG_TIMER_CREATE_REQ 0x10
#define L_MSG_TIMER_CANCEL_REQ 0x11
#define L_MSG_TIMER_FUNC_CALL  0x12
#define L_MSG_TIMER_CREATE_RSP (L_CORE_MSG_GR + 0x1A)
#define L_MSG_TIMER_NOTIFY_IND (L_CORE_MSG_GR + 0x1B)

#define L_MSG_SOCK_ACCEPT_IND 0x20 /* master -> socket service */
#define L_MSG_SOCK_CONN_RSP   0x21
#define L_MSG_SOCK_READY_TX   0x22
#define L_MSG_SOCK_READY_RX   0x23
#define L_MSG_SOCK_DISC_NTF   0x24
#define L_MSG_SOCK_ERROR_NTF  0x25

#define L_MSG_SOCK_CONN_NTF    0x30 /* listen service <-> socket service */
#define L_MSG_SOCK_DISC_CMD    0x31
#define L_MSG_STOP_SKLS_CMD    0x32

#define L_MSG_SOCK_DATA_TX_REQ 0x33 /* user service -> socket service */
#define L_MSG_SOCK_DATA_RX_REQ 0x34
#define L_MSG_SOCK_RECOVER_REQ 0x35
#define L_MSG_SOCK_CLOSE_CMD   0x36

#define L_MSG_SOCK_READY_NTF     (L_CORE_MSG_GR + 0x2A) /* socket service -> user service */
#define L_MSG_SOCK_READ_RSP      (L_CORE_MSG_GR + 0x2B)
#define L_MSG_SOCK_WRITE_RSP     (L_CORE_MSG_GR + 0x2C)
#define L_MSG_SOCK_BAD_STATE_NTF (L_CORE_MSG_GR + 0x2D)
#define L_MSG_SOCK_RECOVER_RSP   (L_CORE_MSG_GR + 0x2E)

struct l_master;
struct l_thread;
struct l_service;
struct l_message;
struct l_timestamp;
struct l_corotable;
struct l_coroutine;
struct l_svaptable;

typedef struct lnlylib_env {
  struct l_master* M;
  struct l_thread* T;
  struct l_service* S;
  struct l_message* MSG;
  void* svud;
  void* alloc;
  l_ostream* logout;
  struct l_timestamp* stamp;
  struct l_corotable* ctbl;
  struct l_coroutine* coro;
  l_sbuf32 tmstr;
} lnlylib_env;

typedef struct l_thread {
  l_thrhdl thrhdl;
  l_umedit thridx;
  int (*start)(lnlylib_env*);
  lnlylib_env* E;
  void* thrd_alloc;
  l_ostream logout;
} l_thread;

typedef struct {
  l_medit num_workers;
  l_medit init_stbl_size;
  l_sbuf1k start_script;
  l_sbuf1k logfilename;
} l_config;

typedef struct {
  l_smplnode node; /* chained to free q */
  l_squeue bkmq; /* service backup message q */
  struct l_service* service;
  l_umedit seed_num;
  l_ushort flags;
  l_ushort events;
} l_srvcslot;

typedef struct {
  l_medit capacity;
  l_medit num_services;
  l_squeue free_slots;
  l_srvcslot* slot_arr;
} l_srvctable;

typedef struct l_timestamp {
  l_long base_syst_time;
  l_long base_mono_time;
  l_long mast_time_ms;
  l_rwlock* tmlk;
  l_long time_ms;
  l_sbuf32 tmstr;
  l_rwlock time_lock;
} l_timestamp;

#define L_MAX_TIMEQ_SIZE (1024*4) /* should be 2^n */

typedef struct {
  l_long base_ms;
  l_squeue* cur_msec;
  l_squeue msecq[L_MAX_TIMEQ_SIZE]; /* max 4096ms */
} l_timer_msec_level;

typedef struct {
  l_long base_hms;
  l_squeue* cur_hms;
  l_squeue hmsq[L_MAX_TIMEQ_SIZE]; /* max 409600ms */
} l_timer_hms_level;

typedef struct {
  l_long base_sec;
  l_squeue* cur_sec;
  l_squeue secq[L_MAX_TIMEQ_SIZE]; /* max 4096s */
} l_timer_sec_level;

typedef struct l_timer_create_req {
  l_long diff_ms;
  l_ulong svid;
  l_uint tmud;
  l_long times;
  l_ulong count;
  void (*func)(lnlylib_env*, void*);
  void* parm;
} l_timer_create_req;

typedef struct {
  l_timer timer;
} l_timer_cancel_req;

typedef struct {
  void (*func)(lnlylib_env*, void*);
  void* parm;
} l_timer_func_call;

typedef struct {
  l_smplnode node;
  l_timer timer;
  l_long left_msecs;
  l_long expire_time;
  l_timer_create_req data;
} l_timer_node;

typedef struct {
  l_medit capacity;
  l_medit num_timers;
  l_umedit timer_seed;
  l_squeue free_timers;
  l_timer_node* timer_arr;
  l_timer_msec_level* msec_level;
  l_timer_hms_level* hms_level;
  l_timer_sec_level* sec_level;
  l_timer_msec_level msec_level_timers;
  l_timer_hms_level hms_level_timers;
  l_timer_sec_level sec_level_timers;
} l_timertable;

typedef struct {
  struct l_worker* worker;
  l_squeue mast_rxmq;
  l_mutex mast_rxlk;
} l_worknode;

typedef struct {
} l_cmdline;

typedef struct l_master {
  l_thread T;
  l_config* conf;
  l_cmdline* cmds;
  l_mutex* qlock;
  l_condv* qcndv;
  l_squeue* globalq;
  l_squeue* mast_frmq;
  l_squeue* mast_frsq;
  l_squeue* temp_svcq;
  lnlylib_env* E;
  l_timestamp* stamp;
  l_srvctable* stbl;
  l_timertable* ttbl;
  l_umedit srvc_seed;
  l_medit num_workers;
  l_worknode* node_arr;
  l_ioevmgr* evmgr;
  l_squeue queue[4];
  l_srvctable srvc_tbl;
  lnlylib_env main_env;
  l_config config;
  l_cmdline cmdline;
  l_mutex globalq_lock;
  l_condv globalq_cndv;
  l_timestamp timestamp;
  l_ioevmgr ioevent_mgr;
  l_timertable timer_tbl;
} l_master;

typedef struct l_worker {
  l_thread T;
  l_umedit work_flags;
  l_medit weight;
  l_squeue* work_frmq;
  l_squeue* srvc_to_self; /* msgs send to current service */
  l_squeue* srvc_to_srvc; /* msgs send to other services */
  l_squeue* srvc_to_mast; /* msgs from service to master */
  l_squeue* mast_rxmq; /* msgs from worker to master */
  l_mutex* mast_rxlk;
  l_squeue msgq[4];
  lnlylib_env work_env;
} l_worker;

typedef struct {
  struct l_service* service;
  l_umedit thridx;
  l_squeue srvc_to_srvc;
  l_squeue srvc_to_mast;
} l_service_round_fin;

typedef struct {
  struct l_service* killer;
  l_umedit thridx;
} l_worker_exit_rsp;

typedef struct l_coroutine {
  l_smplnode node; /* chain to free q */
  l_umedit seed_num;
  l_umedit wait_mgid;
  l_umedit session;
  int coref;
  lua_State* co;
} l_coroutine;

/* lua state need about 5K memory, and a coroutine need about 1K memory */
typedef struct l_corotable {
  l_smplnode node;
  l_umedit capacity;
  l_umedit coro_seed;
  l_squeue free_coro;
  l_coroutine* coro_arr;
  lua_State* L;
} l_corotable;

typedef struct l_service {
  l_smplnode node; /* chained in global q */
  l_squeue srvc_msgq;
  l_filehdl ioev_hdl;
  l_umedit srvc_flags;
  l_ulong from_id; /* who created this service */
  l_ulong srvc_id; /* the highest bit is for remote service or not, high 32-bit is the index (cannot be 0), low 32-bit is the seed num */
  l_corotable* coro_tabl; /* only created for lua service */
  l_service_callback* srvc_cb;
  void* srvc_ud;
  struct l_svaptable* svap_tbl;
} l_service;

typedef struct {
  l_service_create_para para;
  l_service_access_point* sap;
} l_service_create_req;

typedef struct {
  l_ulong svid;
} l_service_stop_req;

typedef struct {
  l_umedit a, b, c, d;
  l_ulong l, m, n, o;
  l_uint u, v, w, x;
  void *p, *q, *r, *s;
} l_msgdata;

typedef struct l_message {
  l_smplnode node;
  l_service_access_point* dest_apid;
  l_service_access_point* from_apid;
  l_ulong dest_svid;
  l_ulong dest_coro;
  l_ulong from_svid;
  l_ulong from_coro;
  l_umedit session;
  l_umedit mssg_id;
  l_umedit mssg_flags;
  l_umedit data_size;
  void* mssg_data;
  l_msgdata extra;
} l_message;

static l_message* l_master_create_message(l_master* M, l_ulong dest_svid, l_umedit mgid, void* data, l_umedit size);
static l_message* l_create_message(lnlylib_env* E, l_ulong dest_svid, l_umedit mgid, void* data, l_umedit size);
static void l_message_free_data(lnlylib_env* E, l_message* msg);

static l_medit l_service_get_slot_index(l_ulong svid);
static l_service* l_master_create_service(l_master* M, l_medit svid, l_service_create_para para);
static void l_master_stop_service(l_master* M, l_ulong srvc_id);
static void l_master_destroy_service(l_master* M, l_ulong srvc_id);
static void l_master_free_service_slot(l_master* M, l_srvcslot* slot, l_service* S);
static void l_service_add_access_point(lnlylib_env* E, l_service* S, l_service_access_point* sap);
static l_service_access_point* l_service_find_access_point(lnlylib_env* E, l_service* S, l_ulong remote_svid);

struct l_timer_create_req;
static void l_master_create_timer(l_master* M, struct l_timer_create_req* req);
static void l_master_cancel_timer(l_master* M, l_timer timer);
static void l_master_check_timers(l_master* M);
static void l_master_update_time(l_master* M);

static void l_finish_logging(lnlylib_env* E);
static l_int l_impl_file_write(void* out, const void* p, l_int len);
static void l_impl_file_flush(void* out);

typedef struct {
} l_thrdalloc;

static void
l_thrdalloc_init(l_thrdalloc* a)
{
  L_UNUSED(a);
}

static l_thrdalloc*
l_thrdalloc_create()
{
  return 0;
}

static void
l_thrdalloc_destroy(l_thrdalloc* a)
{
  L_UNUSED(a);
}

static l_master* L_MASTER;

#if defined(L_THREAD_LOCAL)
static L_THREAD_LOCAL lnlylib_env* L_ENV;
#else
l_thrkey L_ENV_THRKEY;
#endif

static void
l_threadlocal_prepare()
{
#if defined(L_THREAD_LOCAL)
  L_ENV = 0;
#else
  l_thrkey_init(&L_ENV_THRKEY);
  l_thrkey_set_data(&L_ENV_THRKEY, 0);
#endif
}

static void
l_threadlocal_release()
{
#if defined(L_THREAD_LOCAL)
  L_ENV = 0;
#else
  l_thrkey_free(&L_ENV_THRKEY);
#endif
}

static void
l_threadlocal_set(lnlylib_env* E)
{
#if defined(L_THREAD_LOCAL)
  L_ENV = E;
#else
  l_thrkey_set_data(&L_ENV_THRKEY, E);
#endif
}

static lnlylib_env*
l_threadlocal_get()
{
#if defined(L_THREAD_LOCAL)
  return L_ENV;
#else
  return l_thrkey_get_data(&L_ENV_THRKEY);
#endif
}

lnlylib_env* l_get_lnlylib_env()
{
  return l_threadlocal_get();
}

static void
l_config_load(l_config* conf)
{
  lua_State* L = l_new_state();
  l_funcindex func;
  l_tableindex env;
  l_strbuf* script_buf = 0;
  l_strbuf* logfile_buf = 0;
  l_strn script, logfile;

  conf->num_workers = 1;
  conf->init_stbl_size = L_SRVC_MIN_TABLE_SIZE;
  script_buf = l_sbuf1k_init(&conf->start_script);
  logfile_buf = l_sbuf1k_init_from(&conf->logfilename, l_literal_strn("stdout"));

  func = l_load_file(L, LNLYLIB_HOME_DIR "/conf/config.lua");
  if (func.index <= 0) {
    l_loge_s(LNUL, "load config file failed");
    return;
  }

  env = l_new_table(L);
  if (env.index <= 0) {
    l_loge_s(LNUL, "create env table failed");
    return;
  }

  l_set_funcenv(L, func, env);
  func.index = l_push_value(L, func.index);
  l_pcall_func(L, func, 0);

  conf->num_workers = l_table_get_int(L, env, "workers");
  if (conf->num_workers < 1) {
    conf->num_workers = 1;
  }

  conf->init_stbl_size = l_table_get_int(L, env, "services");
  if (conf->init_stbl_size < L_SRVC_MIN_TABLE_SIZE) {
    conf->init_stbl_size = L_SRVC_MIN_TABLE_SIZE;
  }

  script = l_table_get_str(L, env, "script");
  l_strbuf_reset(script_buf, script);

  logfile = l_table_get_str(L, env, "logfile");
  if (l_strbuf_reset(logfile_buf, logfile) == 0) {
    l_strbuf_reset(logfile_buf, l_literal_strn("stdout"));
  }

  l_close_state(L);
}

static l_ostream
l_config_logout(l_config* conf, l_umedit thridx)
{
  l_ostream logout;
  l_strn prefix;
  prefix = l_strbuf_strn(l_sbuf1k_p(&conf->logfilename));

  if (l_strn_equal(&prefix, l_literal_strn("stdout"))) {
    logout = l_stdout_ostream();
  } else if (l_strn_equal(&prefix, l_literal_strn("stderr"))) {
    logout = l_stderr_ostream();
  } else {
    l_sbuf2k name_buf;
    l_strbuf* p_name_buf = 0;
    l_ostream name_out;
    p_name_buf = l_sbuf2k_init(&name_buf);
    name_out = l_strbuf_ostream(p_name_buf);
    if (l_ostream_format_2(&name_out, "%strn.%d.log", lstrn(&prefix), ld(thridx)) != 2) {
      l_loge_2(LNUL, "create log ostream fail %strn %d", lstrn(&prefix), ld(thridx));
      logout = l_stdout_ostream();
    } else {
      logout = l_ostream_from(fopen((const char*)l_strbuf_strc(p_name_buf), "ab"), l_impl_file_write, l_impl_file_flush);
      if (logout.out == 0) {
        l_loge_1(LNUL, "open log file fail %s", ls(l_strbuf_strc(p_name_buf)));
        logout = l_stdout_ostream();
      }
    }
  }
  l_ostream_format_1(&logout, "%s", ls(L_NEWLINE "--------" L_NEWLINE));
  return logout;
}

static void
l_thread_init(l_thread* T, l_umedit thridx, lnlylib_env* env, l_config* conf)
{
  T->thridx = thridx;
  T->start = 0;
  T->E = env;
  if (conf == 0) {
    T->logout = l_stdout_ostream();
    T->thrd_alloc = 0;
  } else {
    T->logout = l_config_logout(conf, thridx);
    T->thrd_alloc = l_thrdalloc_create();
  }
}

static void
l_thread_free(l_thread* T)
{
  if (T->logout.out && T->logout.out != stdout && T->logout.out != stderr) {
    fclose((FILE*)T->logout.out);
    T->logout.out = 0;
  }

  if (T->thrd_alloc) {
    l_thrdalloc_destroy(T->thrd_alloc);
    T->thrd_alloc = 0;
  }
}

static void*
l_thread_proc(void* para)
{
  void* exit_code = 0;
  lnlylib_env* env = (lnlylib_env*)para;
  l_threadlocal_set(env);
  exit_code = (void*)(l_int)env->T->start(env);
  l_finish_logging(env);
  l_thread_free(env->T);
  return exit_code;
}

static void
l_thread_start(l_thread* T, int (*start)(lnlylib_env*))
{
  T->start = start;
  l_thrhdl_create(&T->thrhdl, l_thread_proc, T->E);
}

static void
l_thread_join(l_thread* T)
{
  l_thrhdl_join(&T->thrhdl);
}

/** service init and free **/

static void
l_service_init(l_service* S, l_medit svid, l_umedit seed)
{
  l_squeue_init(&S->srvc_msgq);
  S->ioev_hdl = L_EMPTY_HDL;
  S->srvc_flags = 0;
  S->from_id = 0;
  S->srvc_id = (((l_ulong)svid) << 32) | seed;
  S->coro_tabl = 0;
  S->srvc_cb = 0;
  S->srvc_ud = 0;
}

static void
l_service_free_co(l_service* S)
{
  L_UNUSED(S);
}

static void
l_srvcslot_init(l_srvcslot* slot)
{
  l_squeue_init(&slot->bkmq);
  slot->service = 0;
  slot->seed_num = 0;
  slot->flags = 0;
  slot->events = 0;
}

static void
l_srvctable_init(l_master* M, l_srvctable* stbl, l_medit init_size)
{
  l_srvcslot* slot = 0;
  l_medit i = 0;

  stbl->capacity = init_size;
  stbl->num_services = 0;
  l_squeue_init(&stbl->free_slots);
  stbl->slot_arr = L_MALLOC_TYPE_N(M->E, l_srvcslot, stbl->capacity);

  for (i = 0; i < stbl->capacity; ++i) {
    slot = stbl->slot_arr + i;
    l_srvcslot_init(slot);

    if (i >= L_SRVC_MIN_USER_SVID) {
      l_squeue_push(&stbl->free_slots, &slot->node);
    }
  }
}

/** time and timer init and free **/

static l_sbuf32
l_timestamp_gen_str(l_long ms)
{
  l_sbuf32 str;
  l_strbuf* p_str = 0;
  l_ostream str_out;
  l_value date[8] = {0};

  p_str = l_sbuf32_init(&str);
  str_out = l_strbuf_ostream(p_str);

  l_timestamp_from_msec(ms, date);
  l_ostream_format_n(&str_out, "%04d/%02d/%02d %02d:%02d:%02d.%03d", 7, date);

  return str;
}

static void
l_timestamp_init(l_timestamp* stamp)
{
  stamp->base_syst_time = l_system_time_ms();
  stamp->base_mono_time = l_mono_time_ms();
  stamp->mast_time_ms = stamp->base_syst_time;

  stamp->tmlk = &stamp->time_lock;
  l_rwlock_init(stamp->tmlk);

  stamp->time_ms = stamp->base_syst_time;
  stamp->tmstr = l_timestamp_gen_str(stamp->time_ms);
}

static void
l_timertable_init(l_master* M, l_timertable* ttbl, l_medit n)
{
  l_medit i = 0;
  l_timer_node* node = 0;
  l_long base_ms = 0;

  l_zero_n(ttbl, sizeof(l_timertable));

  if (n < L_TIMER_MIN_TABLE_SIZE) {
    n = L_TIMER_MIN_TABLE_SIZE;
  }

  ttbl->capacity = n;
  ttbl->num_timers = 0;
  ttbl->timer_seed = 0;
  l_squeue_init(&ttbl->free_timers);
  ttbl->timer_arr = L_MALLOC_TYPE_N(M->E, l_timer_node, n);

  for (; i < n; ++i) {
    node = ttbl->timer_arr + i;
    l_squeue_push(&ttbl->free_timers, &node->node);
  }

  base_ms = M->stamp->mast_time_ms;

  ttbl->msec_level = &ttbl->msec_level_timers;
  ttbl->msec_level->base_ms = base_ms;
  ttbl->msec_level->cur_msec = ttbl->msec_level->msecq;

  ttbl->hms_level = &ttbl->hms_level_timers;
  ttbl->hms_level->base_hms = base_ms / 100;
  ttbl->hms_level->cur_hms = ttbl->hms_level->hmsq;

  ttbl->sec_level = &ttbl->sec_level_timers;
  ttbl->sec_level->base_sec = base_ms / 1000;
  ttbl->sec_level->cur_sec = ttbl->sec_level->secq;
}

static void
l_timertable_free(l_timertable* ttbl)
{
  L_UNUSED(ttbl);
}

/** master init and free **/

static void
l_parse_cmd_line(l_master* M, int argc, char** argv)
{
  L_UNUSED(M);
  L_UNUSED(argc);
  L_UNUSED(argv);
}

static lnlylib_env*
l_master_init(int (*start)(lnlylib_env*), int argc, char** argv)
{
  l_master* M = 0;
  l_thrdalloc thrd_alloc;
  lnlylib_env* main_env = 0;
  l_config* conf = 0;
  l_worknode* work_node = 0;
  l_worker* worker = 0;
  l_medit i = 0;

  M = L_MALLOC_TYPE(LNUL, l_master);
  L_MASTER = M;

  l_zero_n(M, sizeof(l_master));

  M->conf = &M->config;
  M->cmds = &M->cmdline;

  M->stamp = &M->timestamp;
  l_timestamp_init(M->stamp);

  M->qlock = &M->globalq_lock;
  M->qcndv = &M->globalq_cndv;
  l_mutex_init(M->qlock);
  l_condv_init(M->qcndv);

  l_squeue_init(M->queue + 0);
  l_squeue_init(M->queue + 1);
  l_squeue_init(M->queue + 2);
  l_squeue_init(M->queue + 3);
  M->globalq = M->queue + 0;
  M->mast_frmq = M->queue + 1;
  M->mast_frsq = M->queue + 2;
  M->temp_svcq = M->queue + 3;
  M->E = &M->main_env;

  l_thread_init(&M->T,  L_THREAD_MASTER, M->E, 0);
  l_thrdalloc_init(&thrd_alloc);
  M->T.thrd_alloc = &thrd_alloc;
  M->T.thrhdl = l_thrhdl_self();
  M->T.start = start;

  main_env = M->E;
  main_env->M = M;
  main_env->T = &M->T;
  main_env->alloc = M->T.thrd_alloc;
  main_env->logout = &M->T.logout;
  main_env->stamp = M->stamp;

  l_threadlocal_set(main_env);

  conf = M->conf;
  l_config_load(conf);
  M->T.logout = l_config_logout(conf, L_THREAD_MASTER);
  M->T.thrd_alloc = l_thrdalloc_create();
  main_env->logout = &M->T.logout;
  main_env->alloc = M->T.thrd_alloc;

  l_parse_cmd_line(M, argc, argv);

  /* init service table */

  M->srvc_seed = 0;
  M->stbl = &M->srvc_tbl;
  l_srvctable_init(M, M->stbl, conf->init_stbl_size);

  /* init timer table */

  M->ttbl = &M->timer_tbl;
  l_timertable_init(M, M->ttbl, L_TIMER_MIN_TABLE_SIZE);

  /* init worker threads */

  M->num_workers = conf->num_workers;
  M->node_arr = L_MALLOC_TYPE_N(main_env, l_worknode, M->num_workers);

  for (i = 0; i < M->num_workers; ++i) {
    work_node = M->node_arr + i;
    worker = L_MALLOC_TYPE(main_env, l_worker);
    l_thread_init(&worker->T, L_THREAD_WORKER + i, &worker->work_env, conf);
    worker->work_flags = 0;
    worker->weight = i / 4 - 1;

    l_squeue_init(worker->msgq + 0);
    l_squeue_init(worker->msgq + 1);
    l_squeue_init(worker->msgq + 2);
    l_squeue_init(worker->msgq + 3);
    worker->work_frmq = worker->msgq + 0;
    worker->srvc_to_self = worker->msgq + 1;
    worker->srvc_to_srvc = worker->msgq + 2;
    worker->srvc_to_mast = worker->msgq + 3;

    worker->mast_rxmq = &work_node->mast_rxmq;
    worker->mast_rxlk = &work_node->mast_rxlk;
    l_squeue_init(worker->mast_rxmq);
    l_mutex_init(worker->mast_rxlk);

    worker->work_env.M = M;
    worker->work_env.T = &worker->T;
    worker->work_env.S = 0;
    worker->work_env.MSG = 0;
    worker->work_env.svud = 0;
    worker->work_env.alloc = worker->T.thrd_alloc;
    worker->work_env.ctbl = 0;
    worker->work_env.coro = 0;
    worker->work_env.logout = &worker->T.logout;
    worker->work_env.stamp = M->stamp;

    work_node->worker = worker;
  }

  /* io event manager */
  M->evmgr = 0;

  return main_env;
}

static void
l_program_exit(lnlylib_env* E)
{
  l_worker* W = (l_worker*)E->T;
  W->work_flags |= L_WORK_FLAG_PROGRAM_EXIT;
}

static void
l_master_clean(lnlylib_env* main_env)
{
  l_master* M = main_env->M; /* TODO */
  l_thread_free(&M->T);
}

/** master handling **/

static void
l_master_insert_message(l_master* M, l_message* msg)
{
  l_srvctable* stbl = &M->srvc_tbl;
  l_medit dest_svid = l_service_get_slot_index(msg->dest_svid);
  l_srvcslot* srvc_slot = 0;

  srvc_slot = stbl->slot_arr + dest_svid;
  if (dest_svid >= stbl->capacity || (srvc_slot->flags & L_SERVICE_ALIVE) == 0) {
    /* invalid message, just insert into free q */
    l_loge_3(M->E, "invalid message %x from %d to %d", lx(msg->mssg_id), ld(msg->from_svid), ld(dest_svid));
    l_message_free_data(M->E, msg);
    l_squeue_push(M->mast_frmq, &msg->node);
  } else {
    l_service* S = 0;
    if ((S = srvc_slot->service)) {
      /* service is docked in the service table, docked service is waiting to handle,
         push the message to service's message q and insert the service to temp q to handle */
      l_squeue_push(&S->srvc_msgq, &msg->node);
      if ((S->srvc_flags & L_DOCKED_SERVICE_IN_TEMPQ) == 0) {
        l_squeue_push(M->temp_svcq, &S->node);
        S->srvc_flags |= L_DOCKED_SERVICE_IN_TEMPQ;
      }
    } else {
      /* service is already in global q or is handling in a worker thread,
         just push the message to its backup message queue */
      l_squeue_push(&srvc_slot->bkmq, &msg->node);
    }
  }
}

static void
l_master_generate_io_messages(l_service* S, l_srvcslot* slot)
{
  l_master* M = L_MASTER;
  l_squeue* srvc_msgq = &S->srvc_msgq;
  l_ushort events = 0;
  l_message* msg = 0;

  events = slot->events;
  slot->events = 0;

  if (events == 0 || l_filehdl_is_empty(&S->ioev_hdl)) {
    return;
  }

  if (events & L_IO_EVENT_WRITE) {
    if (slot->flags & L_SRVC_FLAG_SOCK_CONNECT) {
      if (l_socket_cmpl_connect(S->ioev_hdl)) {
        msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_CONN_RSP, 0, slot->flags & L_SRVC_FLAG_SOCK_PENDING);
        l_squeue_push(srvc_msgq, &msg->node);
        msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_READY_TX, 0, 0);
        l_squeue_push(srvc_msgq, &msg->node);
      } else {
        msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_DISC_NTF, 0, 0);
        l_squeue_push(srvc_msgq, &msg->node);
      }
      slot->flags &= ~(L_SRVC_FLAG_SOCK_CONNECT | L_SRVC_FLAG_SOCK_PENDING);
    } else {
      msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_READY_TX, 0, 0);
      l_squeue_push(srvc_msgq, &msg->node);
    }
  }

  if (events & L_IO_EVENT_READ) {
    if (slot->flags & L_SRVC_FLAG_SOCK_LISTEN) {
      msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_ACCEPT_IND, 0, 0);
      l_squeue_push(srvc_msgq, &msg->node);
    } else {
      msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_READY_RX, 0, 0);
      l_squeue_push(srvc_msgq, &msg->node);
    }
  }

  if (events & (L_IO_EVENT_HUP | L_IO_EVENT_RDH)) {
    msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_DISC_NTF, 0, events & L_IO_EVENT_RDH);
    l_squeue_push(srvc_msgq, &msg->node);
  }

  if (events & L_IO_EVENT_ERR) {
    msg = l_master_create_message(M, S->srvc_id, L_MSG_SOCK_ERROR_NTF, 0, 0);
    l_squeue_push(srvc_msgq, &msg->node);
  }
}

static void
l_master_dispatch_io_event(l_ulong ud, l_ushort events)
{
  l_master* M = L_MASTER;
  l_srvctable* stbl = M->stbl;
  l_squeue* temp_svcq = M->temp_svcq;
  l_service* S = 0;
  l_srvcslot* slot = 0;
  l_medit svid = l_service_get_slot_index(ud);
  l_umedit seed = (l_umedit)(ud & 0xffffffff);

  slot = stbl->slot_arr + svid;
  if (events == 0 || (slot->flags & L_SERVICE_ALIVE) == 0 || slot->seed_num != seed) {
    return;
  }

  slot->events |= events;

  S = slot->service;
  if (S == 0) { /* currently the service is waiting for handle or is hanlding, */
    return;   /* the events are handle when the service finish the current handle */
  }

  l_master_generate_io_messages(S, slot);

  if ((S->srvc_flags & L_DOCKED_SERVICE_IN_TEMPQ) == 0) {
    S->srvc_flags |= L_DOCKED_SERVICE_IN_TEMPQ;
    l_squeue_push(temp_svcq, &S->node);
  }
}

static void
l_master_deliver_messages(l_master* M, l_squeue* txmq)
{
  l_message* msg = 0;
  while ((msg = (l_message*)l_squeue_pop(txmq))) {
    l_logv_3(M->E, "master deliver message %x from %svid to %svid", lx(msg->mssg_id), lx(msg->from_svid), lx(msg->dest_svid));
    l_master_insert_message(M, msg);
  }
}

static void
l_master_handle_messages(l_master* M, l_squeue* txms)
{
  l_message* msg = 0;

  while ((msg = (l_message*)l_squeue_pop(txms))) {
    l_logv_2(M->E, "master handle message %x from %svid", lx(msg->mssg_id), lx(msg->from_svid));
    switch (msg->mssg_id) {
    case L_MSG_SERVICE_CREATE_REQ: {
      l_service* S = 0;
      l_service_create_req* req = 0;
      l_subsrvc_create_rsp rsp;
      l_message* rsp_msg = 0;
      l_message* on_create = 0;

      req = (l_service_create_req*)msg->mssg_data;
      S = l_master_create_service(M, 0, req->para);

      if (S) {
        /* the 1st message for the new service */
        S->from_id = msg->from_svid;
        on_create = l_master_create_message(M, S->srvc_id, L_MSG_SERVICE_ON_CREATE, 0, 0);
        l_master_insert_message(M, on_create);

        /* report service created success */
        rsp.svid = S->srvc_id;
        rsp.svud = S->srvc_ud;
        rsp.sap = req->sap ? req->sap : 0;
        rsp_msg = l_master_create_message(M, msg->from_svid, L_MSG_SUBSRVC_CREATE_RSP, &rsp, sizeof(l_subsrvc_create_rsp));
        l_master_insert_message(M, rsp_msg);
      } else {
        /* report service created failed */
        rsp.svid = 0;
        rsp.svud = req->para.svud;
        rsp.sap = req->sap ? req->sap : 0;
        rsp_msg = l_master_create_message(M, msg->from_svid, L_MSG_SUBSRVC_CREATE_RSP, &rsp, sizeof(l_subsrvc_create_rsp));
        l_master_insert_message(M, rsp_msg);
      }}
      break;
    case L_MSG_SERVICE_STOP_REQ:
      l_master_stop_service(M, msg->dest_svid);
      break;
    case L_MSG_TIMER_CREATE_REQ:
      l_master_create_timer(M, (l_timer_create_req*)msg->mssg_data);
      break;
    case L_MSG_TIMER_CANCEL_REQ:
      l_master_cancel_timer(M, ((l_timer_cancel_req*)msg->mssg_data)->timer);
      break;
    default:
      l_loge_2(M->E, "unrecognized service %svid to master message %x", lx(msg->from_svid), lx(msg->mssg_id));
      break;
    }

    l_message_free_data(M->E, msg);
    l_squeue_push(M->mast_frmq, &msg->node);
  }
}

static void*
l_booter_on_create(lnlylib_env* E)
{
  l_master* M = E->M;
  l_config* conf = M->conf;
  l_strn script = l_strbuf_strn(l_sbuf1k_p(&conf->start_script));

  l_logm_s(E, "booter on create");

  if (l_strn_nt_empty(&script)) {
    /* TODO: create a lua service and run the script */
  }

  if (M->T.start) {
    M->T.start(E);
  }

  l_stop_service(E);
  return 0;
}

static void
l_booter_on_destroy(lnlylib_env* E)
{
  l_logm_1(E, "booter on destroy (svud %p)", lp(E->svud));
  l_program_exit(E);
}

static void
l_booter_service_proc(lnlylib_env* E)
{
  l_logm_1(E, "booter handle msg %x", lx(E->MSG->mssg_id));
}

static l_service_define
l_booter_service = {
  l_booter_on_create,
  l_booter_on_destroy,
  l_booter_service_proc
};

L_EXTERN void*
l_empty_on_create(lnlylib_env* E)
{
  L_UNUSED(E);
  return 0;
}

L_EXTERN void
l_empty_on_destroy(lnlylib_env* E)
{
  L_UNUSED(E);
}

L_EXTERN void
l_empty_service_proc(lnlylib_env* E)
{
  L_UNUSED(E);
}

static l_service_define
l_empty_service = {
  l_empty_on_create,
  l_empty_on_destroy,
  l_empty_service_proc
};

static int
l_master_loop(lnlylib_env* main_env)
{
  l_master* M = main_env->M;
  l_squeue* Q = M->globalq;
  l_mutex* QLOCK = M->qlock;
  l_condv* QCNDV = M->qcndv;

  l_srvctable* stbl = M->stbl;
  l_squeue* temp_svcq = M->temp_svcq;
  l_service* S = 0;
  l_srvcslot* srvc_slot = 0;

  l_medit num_workers = M->num_workers;
  l_squeue* mast_frmq = M->mast_frmq;
  l_worknode* work_node = 0;
  l_message* MSG = 0;
  l_medit i = 0;
  int global_q_is_empty = 0;
  int master_exit = 0;

  l_service* booter = 0;
  l_message* on_create = 0;
  l_service* killer = 0;

  l_squeue work_to_mast;
  l_squeue srvc_need_hdl;

  l_squeue_init(&work_to_mast);
  l_squeue_init(&srvc_need_hdl);

  booter = l_master_create_service(M, L_SERVICE_BOOTER, L_SERVICE(&l_booter_service));
  on_create = l_master_create_message(M, booter->srvc_id, L_MSG_SERVICE_ON_CREATE, 0, 0);
  l_squeue_push(&booter->srvc_msgq, &on_create->node);
  stbl->slot_arr[L_SERVICE_BOOTER].service = 0; /* need detach the service from the table first before insert into global q */
  l_squeue_push(&srvc_need_hdl, &booter->node);

  l_socket_prepare();

  for (; ;) {
    if (l_squeue_nt_empty(&srvc_need_hdl)) {
      l_mutex_lock(QLOCK);
      global_q_is_empty = l_squeue_is_empty(Q);
      l_squeue_push_queue(Q, &srvc_need_hdl);
      l_mutex_unlock(QLOCK);

      if (global_q_is_empty) {
        l_condv_broadcast(QCNDV);
      }
    } else {
      l_thread_sleep_ms(1);
    }

    l_master_update_time(M);
    l_master_check_timers(M);

    if (master_exit) {
      break;
    }

    l_logv_s(M->E, "master heart beating ...");

    for (i = 0; i < num_workers; ++i) { /* receive msgs from workers */
      work_node = M->node_arr + i;
      l_mutex_lock(&work_node->mast_rxlk);
      l_squeue_push_queue(&work_to_mast, &work_node->mast_rxmq);
      l_mutex_unlock(&work_node->mast_rxlk);
    }

    while ((MSG = (l_message*)l_squeue_pop(&work_to_mast))) {
      l_logv_2(M->E, "receive worker message %x from %svid", lx(MSG->mssg_id), lx(MSG->from_svid));
      switch (MSG->mssg_id) {
      case L_MSG_PROGRAM_EXIT_REQ:
        /* TODO: send L_MSG_SERVICE_EXIT_REQ to all existing services */
        killer = l_master_create_service(M, L_SERVICE_KILLER, L_SERVICE(&l_empty_service));
        l_squeue_push(&srvc_need_hdl, &killer->node); /* killer service is docked even it is being handled */
        break;
      case L_MSG_PROGRAM_EXIT_RSP:
        /* wait until all workers exit */
        l_assert(M->E, M->num_workers > 0);
        M->num_workers -= 1;
        if (M->num_workers > 0) {
          l_worker_exit_rsp* rsp = 0;
          rsp = (l_worker_exit_rsp*)MSG->mssg_data;
          killer = rsp->killer;
          l_squeue_push(&srvc_need_hdl, &killer->node);
        } else {
          l_master_destroy_service(M, MSG->from_svid); /* destroy the killer service */
          master_exit = 1;
        }
        break;
      case L_MSG_SERVICE_ROUND_FIN: {
        l_service_round_fin* fin = 0;
        fin = (l_service_round_fin*)MSG->mssg_data;
        S = fin->service;
        srvc_slot = stbl->slot_arr + l_service_get_slot_index(S->srvc_id);
        srvc_slot->service = S; /* dock the service to the table first */

        /* insert backup q's meesage to service, and add service to tempq to handle */
        l_squeue_push_queue(&S->srvc_msgq, &srvc_slot->bkmq);
        l_assert(M->E, srvc_slot->flags & L_SERVICE_ALIVE);
        l_assert(M->E, (S->srvc_flags & L_DOCKED_SERVICE_IN_TEMPQ) == 0);
        l_squeue_push(temp_svcq, &S->node);
        S->srvc_flags |= L_DOCKED_SERVICE_IN_TEMPQ;

        l_master_deliver_messages(M, &fin->srvc_to_srvc);
        l_master_handle_messages(M, &fin->srvc_to_mast);

        /* handle io events if any */
        if (srvc_slot->events && (srvc_slot->flags & L_SERVICE_ALIVE)) {
          l_master_generate_io_messages(S, srvc_slot);
        }

        /* check stop service flag */
        if (S->srvc_flags & L_SRVC_FLAG_SERVICE_STOP) {
          S->srvc_flags &= (~L_SRVC_FLAG_SERVICE_STOP);
          l_master_stop_service(M, S->srvc_id);
        }

        /* check destroy service flag */
        if (S->srvc_flags & L_SRVC_FLAG_SERVICE_DESTROY) {
          S->srvc_flags &= (~L_SRVC_FLAG_SERVICE_DESTROY);
          l_master_destroy_service(M, S->srvc_id);
        }}
        break;
      default:
        l_loge_1(main_env, "unrecognized worker to master message %x", lx(MSG->mssg_id));
        break;
      }

      l_squeue_push(mast_frmq, &MSG->node); /* TODO: how to free msgdata */
    }

    if (M->evmgr) {
      int num_events = 0;
      num_events = l_ioevmgr_try_wait(M->evmgr, l_master_dispatch_io_event);
      if (num_events > 0) {
        num_events += l_ioevmgr_try_wait(M->evmgr, l_master_dispatch_io_event);
      }
    }

    while ((S = (l_service*)l_squeue_pop(temp_svcq))) {
      srvc_slot = stbl->slot_arr + l_service_get_slot_index(S->srvc_id);
      if (srvc_slot->flags & L_SERVICE_ALIVE) {
        if (l_squeue_nt_empty(&S->srvc_msgq)) {
          srvc_slot->service = 0; /* detach the docked service, it will be inserted into globalq to handle */
          l_squeue_push(&srvc_need_hdl, &S->node);
        } else {
          l_assert(M->E, srvc_slot->service); /* no handled service should be docked */
        }
        S->srvc_flags &= ~L_DOCKED_SERVICE_IN_TEMPQ;
      } else {
        l_master_free_service_slot(M, srvc_slot, S); /* the service is destroyed already */
      }
    }
  }

  return 0;
}

/** worker handling **/

static void
l_worker_to_master_message(l_worker* W, l_message* msg)
{
  /* deliver worker msg to master */
  l_mutex_lock(W->mast_rxlk);
  l_squeue_push(W->mast_rxmq, &msg->node);
  l_mutex_unlock(W->mast_rxlk);
}

static void
l_worker_finish_service_round(lnlylib_env* E)
{
  l_worker* W = (l_worker*)E->T;
  l_service* S = E->S;
  l_message* msg = 0;
  l_service_round_fin fin;

  l_squeue_push_queue(&S->srvc_msgq, W->srvc_to_self);

  fin.service = S;
  fin.thridx = W->T.thridx;
  l_squeue_move_init(&fin.srvc_to_srvc, W->srvc_to_srvc);
  l_squeue_move_init(&fin.srvc_to_mast, W->srvc_to_mast);

  msg = l_create_message(E, 0, L_MSG_SERVICE_ROUND_FIN, &fin, sizeof(l_service_round_fin));
  l_worker_to_master_message(W, msg);
}

static void
l_worker_send_exit_req(lnlylib_env* E)
{
  l_worker* W = (l_worker*)E->T;
  l_message* msg = 0;
  msg = l_create_message(E, 0, L_MSG_PROGRAM_EXIT_REQ, 0, 0);
  l_worker_to_master_message(W, msg);
}

static void
l_worker_send_exit_rsp(lnlylib_env* E)
{
  l_worker* W = (l_worker*)E->T;
  l_service* S = E->S;
  l_message* msg = 0;
  l_worker_exit_rsp rsp;

  rsp.killer = S;
  rsp.thridx = W->T.thridx;
  msg = l_create_message(E, 0, L_MSG_PROGRAM_EXIT_RSP, &rsp, sizeof(l_worker_exit_rsp));
  l_worker_to_master_message(W, msg);
}

static int
l_worker_loop(lnlylib_env* E)
{
  l_worker* W = (l_worker*)E->T;
  l_squeue* Q = E->M->globalq;
  l_mutex* QLOCK = E->M->qlock;
  l_condv* QCNDV = E->M->qcndv;
  l_service_access_point* sap = 0;
  l_service* S = 0;
  l_message* MSG = 0;
  int i = 0, n = 0;

  l_logm_1(E, "worker %d started", ld(W->T.thridx));

  for (; ;) {
    l_mutex_lock(QLOCK);
    while (l_squeue_is_empty(Q)) {
      l_condv_wait(QCNDV, QLOCK);
    }
    S = (l_service*)l_squeue_pop(Q);
    l_mutex_unlock(QLOCK);

    l_logv_s(E, "worker heart beating ...");

    E->S = S;
    E->svud = S->srvc_ud;
    W->work_flags = 0;

    if ((S->srvc_id >> 32) == L_SERVICE_KILLER) {
      l_worker_send_exit_rsp(E);
      break;
    }

    n = W->weight < 0 ? 1 : 1024; /* TODO */

    for (i = 0; i < n; ++i) {
      MSG = (l_message*)l_squeue_pop(&S->srvc_msgq);
      if (MSG == 0) { break; }
      E->MSG = MSG;
      switch (MSG->mssg_id) {
      case L_MSG_SERVICE_ON_CREATE: {
        void* svud = S->srvc_cb->service_on_create(E);
        if (S->srvc_ud == 0) {
          E->svud = S->srvc_ud = svud;
        } else if (svud != 0) {
          l_loge_1(E, "the service %svid user data already exist", lx(S->srvc_id));
        }}
        break;
      case L_MSG_SERVICE_ON_DESTROY:
        S->srvc_cb->service_on_destroy(E);
        S->srvc_flags |= L_SRVC_FLAG_SERVICE_DESTROY;
        break;
      case L_MSG_TIMER_FUNC_CALL: {
        l_timer_func_call* call = 0;
        call = (l_timer_func_call*)MSG->mssg_data;
        call->func(E, call->parm);
        }
        break;
      case L_MSG_SUBSRVC_CREATE_RSP: {
        l_subsrvc_create_rsp* rsp = 0;
        rsp = (l_subsrvc_create_rsp*)MSG->mssg_data;
        if (rsp->sap) {
          rsp->sap->remote_svid = rsp->svid;
          l_service_add_access_point(E, S, rsp->sap);
        }}
        S->srvc_cb->service_proc(E);
        break;
      default:
        if (S->svap_tbl == 0) {
          S->srvc_cb->service_proc(E);
          break;
        }
        if (!(sap = MSG->dest_apid) && !(sap = l_service_find_access_point(E, S, MSG->from_svid))) {
          S->srvc_cb->service_proc(E);
          break;
        }
        if (MSG->from_apid) {
          if (sap->remote_apid) {
            l_logw_2(E, "remote apid is changed from %p to %p", lp(sap->remote_apid), lp(MSG->from_apid));
          }
          sap->remote_apid = MSG->from_apid;
        }
        sap->access_proc(E);
        break;
      }

      if (MSG->mssg_flags & L_MSSG_FLAG_DONT_AUTO_FREE) {
        MSG->mssg_flags &= (~L_MSSG_FLAG_DONT_AUTO_FREE);
      } else {
        l_message_free_data(E, MSG);
        l_squeue_push(W->work_frmq, &MSG->node);
      }
    }

    l_worker_finish_service_round(E);

    if (W->work_flags & L_WORK_FLAG_PROGRAM_EXIT) {
      l_worker_send_exit_req(E);
      W->work_flags &= (~L_WORK_FLAG_PROGRAM_EXIT);
    }
  }

  l_logm_1(E, "worker %d exited", ld(W->T.thridx));
  return 0;
}

/** main function **/

L_EXTERN int
lnlylib_main(int (*start)(lnlylib_env*), int argc, char** argv)
{
  lnlylib_env* main_env = 0;
  l_master* M = 0;
  l_config* conf = 0;
  l_worker* W = 0;
  l_medit num_workers = 0;
  l_medit i = 0;
  int exit_code = 0;

  l_threadlocal_prepare();

  main_env = l_master_init(start, argc, argv);
  M = main_env->M;
  conf = M->conf;

  l_logm_s(main_env, "master started");

  num_workers = M->num_workers;

  l_logm_4(main_env, "workers %d, services %d, logfile %s, script %s", ld(num_workers), ld(conf->init_stbl_size),
    ls(l_strbuf_strc(l_sbuf1k_p(&conf->logfilename))), ls(l_strbuf_strc(l_sbuf1k_p(&conf->start_script))));

  for (i = 0; i < num_workers; ++i) {
    W = M->node_arr[i].worker;
    l_thread_start(&W->T, l_worker_loop);
  }

  exit_code = l_master_loop(main_env);
  l_logm_1(main_env, "master loop complete %d", ld(exit_code));

  for (i = 0; i < num_workers; ++i) {
    W = M->node_arr[i].worker;
    l_thread_join(&W->T);
    l_logm_1(main_env, "worker %d joined", ld(W->T.thridx));
  }

  l_logm_s(main_env, "master exited");
  l_finish_logging(main_env);

  l_master_clean(main_env);
  l_threadlocal_release();
  return 0;
}

/** message handling **/

L_EXTERN l_message*
l_current_message(lnlylib_env* E)
{
  return E->MSG;
}

L_EXTERN l_umedit
l_current_mgid(lnlylib_env* E)
{
  return l_mgid(E->MSG);
}

L_EXTERN l_umedit
l_current_mgss(lnlylib_env* E)
{
  return l_mgss(E->MSG);
}

L_EXTERN void*
l_current_mgdt(lnlylib_env* E)
{
  return l_mgdt(E->MSG);
}

L_EXTERN l_umedit
l_current_mgdt_size(lnlylib_env* E)
{
  return l_mgdt_size(E->MSG);
}

L_EXTERN l_ulong
l_current_mgfr(lnlylib_env* E)
{
  return l_mgfr(E->MSG);
}

L_EXTERN l_umedit
l_mgid(l_message* msg)
{
  return msg->mssg_id;
}

L_EXTERN l_umedit
l_mgss(l_message* msg)
{
  return msg->session;
}

L_EXTERN void*
l_mgdt(l_message* msg)
{
  return msg->mssg_data;
}

L_EXTERN l_umedit
l_mgdt_size(l_message* msg)
{
  return msg->data_size;
}

L_EXTERN l_ulong
l_mgfr(l_message* msg)
{
  return msg->from_svid;
}

static l_bool
l_service_is_remote(l_ulong svid)
{
  return (svid >> 63) == 1;
}

static l_bool
l_service_nt_remote(l_ulong svid)
{
  return (svid >> 63) == 0;
}

static l_message*
l_master_create_message(l_master* M, l_ulong dest_svid, l_umedit mgid, void* data, l_umedit size)
{
  l_message* msg = 0;

  msg = (l_message*)l_squeue_pop(M->mast_frmq);
  if (msg == 0) {
    msg = L_MALLOC_TYPE(M->E, l_message);
  }

  l_assert(M->E, l_service_nt_remote(dest_svid));
  msg->from_svid = 0;
  msg->dest_svid = dest_svid;
  msg->from_coro = 0;
  msg->dest_coro = 0;

  msg->dest_apid = 0;
  msg->from_apid = 0;
  msg->session = 0;

  msg->mssg_id = mgid;
  msg->mssg_flags = 0;

  l_logv_2(M->E, "master create message %x to %svid", lx(mgid), lx(dest_svid));

  if (data && size > 0) {
    msg->data_size = size;
    l_assert(M->E, size <= sizeof(l_msgdata));
    msg->mssg_data = &msg->extra;
    l_copy_n(msg->mssg_data, data, size);
  } else if (size > 0) {
    msg->data_size = 4;
    msg->mssg_data = &msg->extra;
    msg->extra.a = size; /* size as 4-byte data */
  } else {
    msg->data_size = 0;
    msg->mssg_data = 0;
  }

  return msg;
}

static void
l_master_send_message_to_service(l_master* M, l_ulong dest_svid, l_umedit mgid, void* data, l_umedit size)
{
  l_message* msg = l_master_create_message(M, dest_svid, mgid, data, size);
  l_master_insert_message(M, msg);
}

static l_ulong
l_current_coid(lnlylib_env* E)
{
  l_coroutine* co = E->coro;
  if (E->ctbl && co) {
    l_umedit id = co - E->ctbl->coro_arr;
    return (((l_ulong)id) << 32) | co->seed_num;
  } else {
    return 0;
  }
}

static void
l_message_reset(lnlylib_env* E, l_message* msg, l_ulong dest_svid, l_umedit mgid, l_umedit flags, void* data, l_umedit size)
{
  l_service* S = E->S;
  l_message_free_data(E, msg);

  msg->from_svid = S->srvc_id;
  msg->from_coro = l_current_coid(E);

  if (l_service_is_remote(dest_svid)) {
    msg->mssg_flags |= L_MSSG_FLAG_IS_REMOTE_MSSG;
    msg->dest_svid = ((dest_svid << 1) >> 1); /* TODO */
    msg->dest_coro = 0;
  } else {
    msg->mssg_flags &= (~L_MSSG_FLAG_IS_REMOTE_MSSG);
    msg->dest_svid = dest_svid;
    msg->dest_coro = 0;
  }

  msg->dest_apid = 0;
  msg->from_apid = 0;
  msg->session = 0;

  msg->mssg_id = mgid;
  msg->mssg_flags = flags;

  l_logv_3(E, "create message %x from %svid to %svid", lx(mgid), lx(S->srvc_id), lx(dest_svid));

  if (flags & L_MSSG_FLAG_MOVE_MSSG_DATA) {
    flags &= (~L_MSSG_FLAG_MOVE_MSSG_DATA);
    if (data) {
      msg->data_size = size;
      msg->mssg_data = data;
      msg->mssg_flags |= flags;
    } else {
      l_loge_2(E, "move invalid message data %p %d", lp(data), ld(size));
      msg->data_size = 0;
      msg->mssg_data = 0;
    }
  } else {
    if (data && size > 0) {
      msg->data_size = size;
      if (size <= sizeof(l_msgdata)) {
        msg->mssg_data = &msg->extra;
        l_copy_n(msg->mssg_data, data, size);
      } else {
        l_logd_2(E, "message data %d > %d", ld(size), ld(sizeof(l_msgdata)));
        msg->mssg_data = l_malloc(E, size);
        l_copy_n(msg->mssg_data, data, size);
        msg->mssg_flags |= L_MSSG_FLAG_FREE_MSSG_DATA;
      }
    } else {
      msg->data_size = 0;
      msg->mssg_data = 0;
    }
  }
}

static l_message*
l_create_message_impl(lnlylib_env* E, l_ulong dest_svid, l_umedit mgid, l_umedit flags, void* data, l_umedit size)
{
  l_worker* W = (l_worker*)E->T;
  l_message* msg = 0;

  msg = (l_message*)l_squeue_pop(W->work_frmq);
  if (msg == 0) {
    msg = L_MALLOC_TYPE(E, l_message);
  }

  msg->data_size = 0;
  msg->mssg_data = 0;

  l_message_reset(E, msg, dest_svid, mgid, flags, data, size);
  return msg;
}

static void
l_send_message_impl(lnlylib_env* E, l_message* msg)
{
  l_worker* W = (l_worker*)E->T;
  if (msg->mssg_flags & L_MSSG_FLAG_IS_REMOTE_MSSG) {
    /* TODO - send message to harbor service to handle */
  } else if (msg->dest_svid == msg->from_svid) {
    l_squeue_push(W->srvc_to_self, &msg->node);
  } else {
    l_squeue_push(W->srvc_to_srvc, &msg->node);
  }
}

static l_message*
l_create_message(lnlylib_env* E, l_ulong dest_svid, l_umedit mgid, void* data, l_umedit size)
{
  if (size & L_MOVE_DATA_AUTO_FREE) {
    return l_create_message_impl(E, dest_svid, mgid, L_MSSG_FLAG_MOVE_MSSG_DATA | L_MSSG_FLAG_FREE_MSSG_DATA, data, size & 0x3fffffff);
  } else if (size & L_MOVE_DATA_DONT_FREE) {
    return l_create_message_impl(E, dest_svid, mgid, L_MSSG_FLAG_MOVE_MSSG_DATA, data, size & 0x3fffffff);
  } else {
    return l_create_message_impl(E, dest_svid, mgid, 0, data, size);
  }
}

static void
l_send_message_to_master(lnlylib_env* E, l_umedit mgid, void* data, l_umedit size)
{
  l_message* msg = 0;
  l_worker* W = (l_worker*)E->T;
  msg = l_create_message(E, 0, mgid, data, size);
  l_squeue_push(W->srvc_to_mast, &msg->node);
}

static void /* lua message has dest coroutine need to be specified */
l_send_lua_message(lnlylib_env* E, l_ulong dest_svid, l_ulong dest_coro, l_umedit mgid, l_umedit session, void* data, l_umedit size)
{
  if (mgid < L_MSG_MIN_USER_MSG_ID) {
    l_loge_1(E, "invalid message id %d", ld(mgid));
  } else {
    l_message* msg = 0;
    msg = l_create_message(E, dest_svid, mgid, data, size);
    msg->dest_coro = dest_coro;
    msg->session = session;
    l_send_message_impl(E, msg);
  }
}

static void
l_send_message_to_dest(lnlylib_env* E, l_ulong dest_svid, l_service_access_point* dest_apid, l_service_access_point* local_apid, l_umedit session, l_umedit mgid, void* data, l_umedit size)
{
  if (mgid < L_MSG_MIN_USER_MSG_ID) {
    l_loge_1(E, "invalid message id %d", ld(mgid));
  } else {
    l_message* msg = l_create_message(E, dest_svid, mgid, data, size);
    msg->dest_apid = dest_apid;
    msg->from_apid = local_apid;
    msg->session = session;
    l_send_message_impl(E, msg);
  }
}

L_EXTERN void
l_send_response(lnlylib_env* E, l_umedit mgid, void* data, l_umedit size)
{
  l_send_response_for(E, E->MSG, mgid, data, size);
}

L_EXTERN void
l_send_response_for(lnlylib_env* E, l_message* msg, l_umedit mgid, void* data, l_umedit size)
{
  l_send_message_to_dest(E, msg->from_svid, msg->from_apid, msg->dest_apid, msg->session, mgid, data, size);
}

L_EXTERN void
l_send_message(lnlylib_env* E, l_service_access_point* sap, l_umedit session, l_umedit mgid, void* data, l_umedit size)
{
  l_send_message_to_dest(E, sap->remote_svid, sap->remote_apid, sap, session, mgid, data, size);
}

static void
l_message_free_data(lnlylib_env* E, l_message* msg)
{
  if (msg->mssg_data == 0 || msg->mssg_data == &msg->extra) {
    return;
  }

  if (msg->mssg_flags & L_MSSG_FLAG_FREE_MSSG_DATA) {
    l_mfree(E, msg->mssg_data);
  }

  msg->mssg_data = 0;
}

/** lua message handling **/

#if 0
typedef struct {
  lua_Unsigned mgid;
  lua_Unsigned service;
  lua_Unsigned session;
  lua_Unsigned format;
  void* data;
} l_msg_userdata;

/* c function registers for lua */
static const struct luaL_Reg clanglib[] = {
  {"name", cfuncname},
  {"new", cfuncnew},
  {NULL, NULL}
};

int luaopen_clanglib(lua_State* L)
{
  luaL_newlib(L, clanglib);
  return 1;
}

/* add metatable for type check and oop style access */

int luaopen_clanglib(lua_State* L)
{
  luaL_newmetatable(L, "package.clanglib");
  luaL_newlib(L, clanglib);
  return 1;
}

static int clanglibnew(lua_State* L)
{
  // new the object
  luaL_getmetatable(L, "package.clanglib");
  lua_setmetatable(L, -2);
  return 1; /* new userdata is already on the stack */
}

typedef union {
  lua_Integer i;
  lua_Unsigned u;
} l_Integer_Union;

static lua_Unsigned
l_Integer2Unsigned(lua_Integer i)
{
  l_Integer_Union a;
  a.i = i;
  return a.u;
}

static lua_Integer
l_Unsigned2Integer(lua_Unsigned u)
{
  l_Integer_Union a;
  a.u = u;
  return a.i;
}

static l_ulong
l_checkunsigned(lua_State* L, int stackindex)
{
  lua_Integer i = luaL_checkinteger(L, stackindex);
  return (l_ulong)l_Integer2Unsigned(i);
}

typedef struct {
  l_umedit size;
  l_umedit data[1];
} l_packdata;

static int
l_send_msg(lua_State* co)
{
  lnlylib_env* E = l_get_extra(co);
  l_ulong mgid = l_checkunsigned(co, 1);
  l_ulong dest = l_checkunsigned(co, 2);
  l_umedit flags = (l_umedit)luaL_checkinteger(co, 3);
  l_packdata* pack = (l_packdata*)luaL_checkudata(co, 4, "lnlylib.packdata");
  l_send_lua_message(E, mgid, dest, 0, flags, pack->data, pack->size);
  return 0;
}

local heart = lnlylib_heartbeat
msg = heart.wait_message(msgid) -- the msg content is moved to stack by c layer
send_msg(mgid, dest_svid, flags, data)
send_rsp(msg, mgid, flags, data)

static int
l_send_rsp(lua_State* co)
{
  lnlylib_env* E = l_get_extra(co);
  l_mssgdata* dest = (l_mssgdata*)luaL_checkudata(co, 1, "lnlylib.mssgdata");
  l_ulong mgid = l_checkunsigned(co, 2);
  l_umedit flags = (l_umedit)luaL_checkinteger(co, 3);
  l_packdata* pack = (l_packdata*)luaL_checkudata(co, 4, "lnlylib.packdata");
  l_send_lua_message(E, mgid, dest->service, dest->session, flags, pack->data, pack->size);
  return 0;
}

static int
l_wait_msg(lua_State* co)
{
  lnlylib_env* E = l_get_extra(co);
  l_ulong mgid = l_checkunsigned(co, 1);
  E->coro->wait_mgid = mgid;
  l_assert(LNUL, E->coro->co == co);
  l_yield_impl(co, 0);
  return 0;
}

L_EXTERN void
l_yield_impl(lua_State* co, int nresults)
{
  int status = lua_yield(co, nresults);
  l_loge_1(E, "lua_yield never returns to here %d", ld(status));
}

L_EXTERN int
l_yield(lua_State* co)
{
  return l_yield_impl(co, lua_gettop(L));
}
#endif

/** service handling **/

L_EXTERN l_service*
l_current_service(lnlylib_env* E)
{
  return E->S;
}

L_EXTERN void*
l_current_svud(lnlylib_env* E)
{
  return E->svud;
}

L_EXTERN l_ulong
l_current_svid(lnlylib_env* E)
{
  return l_svid(E->S);
}

L_EXTERN l_filehdl
l_current_evhd(lnlylib_env* E)
{
  return l_evhd(E->S);
}

L_EXTERN l_ulong
l_current_svfr(lnlylib_env* E)
{
  return l_svfr(E->S);
}

L_EXTERN void
l_set_current_svud(lnlylib_env* E, void* svud)
{
  if (svud) {
    E->svud = svud;
    E->S->srvc_ud = svud;
  }
}

L_EXTERN void*
l_svud(l_service* S)
{
  return S->srvc_ud;
}

L_EXTERN l_ulong
l_svid(l_service* S)
{
  return S->srvc_id;
}

L_EXTERN l_filehdl
l_evhd(l_service* S)
{
  return S->ioev_hdl;
}

L_EXTERN l_ulong
l_svfr(l_service* S)
{
  return S->from_id;
}

#define L_MIN_SVAP_TABLE_SIZE 8 /* shall be 2^n */

typedef struct l_svaptable {
  l_umedit capacity;
  l_umedit size;
  l_smplnode slot_arr[L_MIN_SVAP_TABLE_SIZE];
} l_svaptable;

static l_medit
l_service_get_slot_index(l_ulong svid)
{
  return (l_medit)(((svid << 1) >> 1) >> 32);
}

static void
l_svaptable_add_impl(lnlylib_env* E, l_svaptable* svap_tbl, l_service_access_point* sap)
{
  l_smplnode* node = 0;
  l_smplnode* slot_arr = svap_tbl->slot_arr;
  l_ulong remote_svid = sap->remote_svid;

  node = slot_arr + (remote_svid & (svap_tbl->capacity - 1));
  while (node->next && remote_svid < ((l_service_access_point*)node->next)->remote_svid) {
    node = node->next;
  }

  if (remote_svid == ((l_service_access_point*)node->next)->remote_svid) {
    l_loge_1(E, "create multiple access point for service %svid, ignoring it", lx(remote_svid));
    return;
  }

  l_smplnode_insert_after(node, &sap->node);
  svap_tbl->size += 1;
}

static void
l_svaptable_expand(lnlylib_env* E, l_service* S)
{
  l_svaptable* old_tbl = S->svap_tbl;
  l_svaptable* new_tbl = 0;
  l_smplnode* head = 0;
  l_smplnode* slot_arr = 0;
  l_umedit old_bytes = 0;
  l_umedit new_bytes = 0;
  l_umedit i = 0;

  old_bytes = sizeof(l_svaptable) - sizeof(l_smplnode) * L_MIN_SVAP_TABLE_SIZE;
  new_bytes = old_bytes + sizeof(l_smplnode) * old_tbl->capacity * 2;
  old_bytes = old_bytes + sizeof(l_smplnode) * old_tbl->capacity;

  if (new_bytes <= old_bytes) {
    l_loge_1(E, "access point table becomes too large %d", ld(old_tbl->capacity));
    return;
  }

  new_tbl = (l_svaptable*)l_malloc(E, new_bytes);
  l_zero_n(new_tbl, new_bytes);

  new_tbl->capacity = old_tbl->capacity * 2;
  new_tbl->size = 0;

  slot_arr = old_tbl->slot_arr;
  for (; i < old_tbl->capacity; ++i) {
    head = slot_arr + i;
    while (head->next) {
      l_svaptable_add_impl(E, new_tbl, (l_service_access_point*)l_smplnode_remove_next(head));
    }
  }

  l_assert(E, new_tbl->size == old_tbl->size);

  S->svap_tbl = new_tbl;
  l_mfree(E, old_tbl);
}

static void
l_svaptable_add(lnlylib_env* E, l_service* S, l_service_access_point* sap)
{
  l_svaptable* svap_tbl = S->svap_tbl;

  if (svap_tbl->size + 1 >= svap_tbl->capacity) {
    l_svaptable_expand(E, S);
  }

  l_svaptable_add_impl(E, svap_tbl, sap);
}

static void
l_svaptable_create(lnlylib_env* E, l_service* S, l_service_access_point* sap)
{
  l_svaptable* tbl = 0;
  tbl = L_MALLOC_TYPE(E, l_svaptable);
  l_zero_n(tbl, sizeof(l_svaptable));
  tbl->capacity = L_MIN_SVAP_TABLE_SIZE;
  tbl->size = 0;
  l_svaptable_add(E, S, sap);
}

static l_service_access_point*
l_service_del_access_point(lnlylib_env* E, l_service* S, l_service_access_point* sap)
{
  l_ulong remote_svid = sap->remote_svid;
  l_svaptable* svap_tbl = S->svap_tbl;
  l_smplnode* slot_arr = svap_tbl->slot_arr;
  l_smplnode* node = 0;

  node = slot_arr + (remote_svid & (svap_tbl->capacity - 1));

  for (; node->next && remote_svid <= ((l_service_access_point*)node->next)->remote_svid; node = node->next) {
    if (remote_svid == ((l_service_access_point*)node->next)->remote_svid) {
      svap_tbl->size -= 1;
      return (l_service_access_point*)l_smplnode_remove_next(node);
    }
  }

  l_loge_2(E, "cannot remove access point of service %svid for %svid", lx(E->S->srvc_id), lx(remote_svid));
  return 0;
}

static void
l_service_add_access_point(lnlylib_env* E, l_service* S, l_service_access_point* sap)
{
  if (S->svap_tbl == 0) {
    l_svaptable_create(E, S, sap);
  } else {
    l_svaptable_add(E, S, sap);
  }
}

static l_service_access_point*
l_service_find_access_point(lnlylib_env* E, l_service* S, l_ulong remote_svid)
{
  l_svaptable* svap_tbl = S->svap_tbl;
  l_smplnode* slot_arr = svap_tbl->slot_arr;
  l_smplnode* cur_node = 0;

  cur_node = slot_arr[remote_svid & (svap_tbl->capacity - 1)].next;

  for (; cur_node && remote_svid <= ((l_service_access_point*)cur_node)->remote_svid; cur_node = cur_node->next) {
    if (remote_svid == ((l_service_access_point*)cur_node)->remote_svid) {
      return (l_service_access_point*)cur_node;
    }
  }

  L_UNUSED(E);
  return 0;
}

L_EXTERN void
l_create_service_ext(lnlylib_env* E, l_service_create_para para, l_service_access_point* sap, void (*access_proc)(lnlylib_env*))
{
  l_service_create_req req = {para, sap};
  if (sap->remote_svid) {
    l_loge_1(E, "current sap already exist for service %svid", lx(sap->remote_svid));
  } else {
    sap->remote_svid = 0;
    sap->access_proc = access_proc;
    sap->remote_apid = 0;
    l_send_message_to_master(E, L_MSG_SERVICE_CREATE_REQ, &req, sizeof(l_service_create_req));
  }
}

L_EXTERN void
l_create_access_point(lnlylib_env* E, l_service_access_point* sap, l_ulong dest_svid, void (*access_proc)(lnlylib_env*))
{
  /** service access point **
  . create access point is a local behavior, it is not related to remote service
  . it is receiver's responsibilty to map or not map the message to right access point, so we need a way to mark current service is a multi access service or not
  . the sender may not carry the dest access point info, like notification messages (i.e. send from server to client without a client request **/

  if (sap->remote_svid) {
    l_loge_1(E, "current sap already exist for service %svid", lx(sap->remote_svid));
  } else if (l_service_get_slot_index(dest_svid) <= L_SRVC_MAX_CORE_SVID || dest_svid == E->S->srvc_id) {
    l_loge_1(E, "cannot create access point to this service %svid", lx(dest_svid));
  } else {
    l_service* S = E->S;
    sap->remote_svid = dest_svid;
    sap->access_proc = access_proc;
    sap->remote_apid = 0;
    l_service_add_access_point(E, S, sap);
  }
}

L_EXTERN void
l_delete_access_point(lnlylib_env* E, l_service_access_point* sap)
{
  if (sap->remote_svid) {
    l_service_del_access_point(E, E->S, sap);
    sap->remote_svid = 0;
  }
}

L_EXTERN void
l_send_message_to(lnlylib_env* E, l_ulong dest_svid, l_umedit session, l_umedit mgid, void* data, l_umedit size)
{
  l_service* S = E->S;
  if (S->svap_tbl == 0 || dest_svid == S->srvc_id || l_service_get_slot_index(dest_svid) <= L_SRVC_MAX_CORE_SVID) {
    l_send_message_to_dest(E, dest_svid, 0, 0, session, mgid, data, size);
  } else {
    l_service_access_point* sap = 0;
    sap = l_service_find_access_point(E, S, dest_svid);
    if (sap) {
      l_send_message(E, sap, session, mgid, data, size);
    } else {
      l_send_message_to_dest(E, dest_svid, 0, 0, session, mgid, data, size);
    }
  }
}

L_EXTERN l_service_create_para
L_LISTEN_SERVICE(const void* ip, l_ushort port, l_service_callback* cb)
{
  l_service_create_para para;
  para.module = 0;
  para.cb = cb;
  para.enable = true;
  para.listen = true;
  para.hdl = L_EMPTY_HDL;
  para.ip = ip;
  para.port = port;
  para.svud = 0;
  return para;
}

L_EXTERN l_service_create_para
L_LISTEN_MODULE(const void* ip, l_ushort port, const void* module)
{
  l_service_create_para para;
  para.module = module;
  para.cb = 0;
  para.enable = true;
  para.listen = true;
  para.hdl = L_EMPTY_HDL;
  para.ip = ip;
  para.port = port;
  para.svud = 0;
  return para;
}

L_EXTERN l_service_create_para
L_CONNECT_SERVICE(const void* ip, l_ushort port, l_service_callback* cb)
{
  l_service_create_para para;
  para.module = 0;
  para.cb = cb;
  para.enable = true;
  para.listen = false;
  para.hdl = L_EMPTY_HDL;
  para.ip = ip;
  para.port = port;
  para.svud = 0;
  return para;
}

L_EXTERN l_service_create_para
L_CONNECT_MODULE(const void* ip, l_ushort port, const void* module)
{
  l_service_create_para para;
  para.module = module;
  para.cb = 0;
  para.enable = true;
  para.listen = false;
  para.hdl = L_EMPTY_HDL;
  para.ip = ip;
  para.port = port;
  para.svud = 0;
  return para;
}

L_EXTERN l_service_create_para
L_USEHDL_SERVICE(l_filehdl hdl, l_service_callback* cb)
{
  l_service_create_para para;
  para.module = 0;
  para.cb = cb;
  para.enable = true;
  para.listen = false;
  para.hdl = hdl;
  para.ip = 0;
  para.port = 0;
  para.svud = 0;
  return para;
}

L_EXTERN l_service_create_para
L_USERHDL_MODULE(l_filehdl hdl, const void* module)
{
  l_service_create_para para;
  para.module = module;
  para.cb = 0;
  para.enable = true;
  para.listen = false;
  para.hdl = hdl;
  para.ip = 0;
  para.port = 0;
  para.svud = 0;
  return para;
}

L_EXTERN l_service_create_para
L_SERVICE(l_service_callback* cb)
{
  l_service_create_para para;
  para.module = 0;
  para.cb = cb;
  para.enable = false;
  para.listen = false;
  para.hdl = L_EMPTY_HDL;
  para.ip = 0;
  para.port = 0;
  para.svud = 0;
  return para;
}

L_EXTERN l_service_create_para
L_MODULE(const void* module)
{
  l_service_create_para para;
  para.module = module;
  para.cb = 0;
  para.enable = false;
  para.listen = false;
  para.hdl = L_EMPTY_HDL;
  para.ip = 0;
  para.port = 0;
  para.svud = 0;
  return para;
}

static l_umedit
l_gen_srvc_seed(l_master* M)
{
  l_umedit seed = ++M->srvc_seed;
  if (seed == 0) {
    return ++M->srvc_seed;
  } else {
    return seed;
  }
}

static void
l_master_free_service_slot(l_master* M, l_srvcslot* slot, l_service* S)
{
  l_srvctable* stbl = M->stbl;
  l_medit slot_index = slot - stbl->slot_arr;

  l_squeue_push(M->mast_frsq, &S->node);

  if (slot_index >= L_SRVC_MIN_USER_SVID) {
    l_squeue_push(&stbl->free_slots, &slot->node);
  }

  stbl->num_services -= 1;
  l_logm_2(M->E, "service %svid is destroyed (%d)", lx(S->srvc_id), ld(stbl->num_services));
}

static l_srvcslot*
l_master_alloc_service(l_master* M, l_medit svid)
{
  l_srvctable* stbl = M->stbl;
  l_srvcslot* slot = 0;
  l_service* S = 0;

  if (svid > 0) {
    slot = stbl->slot_arr + svid;
    S = (l_service*)l_squeue_pop(M->mast_frsq);
    if (S == 0) {
      S = L_MALLOC_TYPE(M->E, l_service);
    }
    if (S == 0) {
      l_loge_s(M->E, "create service due to alloc fail");
      return 0;
    }
  } else {
    l_squeue* free_slots = &stbl->free_slots;
    l_srvcslot* new_sarr = 0;
    l_medit new_size = 0;
    l_medit i = 0;

    if (l_squeue_is_empty(free_slots)) {
      l_logw_2(M->E, "stbl is full: capacity %d num_services %d", ld(stbl->capacity), ld(stbl->num_services));

      new_size = stbl->capacity * 2;
      if (new_size <= stbl->capacity) {
        l_loge_1(M->E, "current stbl is too large %d", ld(stbl->capacity));
        return 0;
      }

      l_logw_1(M->E, "stbl alloced to new size %d", ld(new_size));
      new_sarr = L_MALLOC_TYPE_N(M->E, l_srvcslot, new_size);

      /* copy the old slots and free the old */

      for (i = 0; i < stbl->capacity; ++i) {
        new_sarr[i] = stbl->slot_arr[i];
      }

      l_mfree(M->E, stbl->slot_arr);

      /* init the new slots */

      for (; i < new_size; ++i) {
        slot = new_sarr + i;
        l_srvcslot_init(slot);

        if (i >= L_SRVC_MIN_USER_SVID) {
          l_squeue_push(free_slots, &slot->node);
        }
      }

      stbl->capacity = new_size;
      stbl->slot_arr = new_sarr;
    }

    slot = (l_srvcslot*)l_squeue_pop(free_slots);
    if (slot == 0) {
      return 0;
    }

    S = (l_service*)l_squeue_pop(M->mast_frsq);
    if (S == 0) {
      S = L_MALLOC_TYPE(M->E, l_service);
    }
    if (S == 0) {
      l_loge_s(M->E, "create service due to alloc fail");
      l_squeue_push(free_slots, &slot->node); /* insert slot back to freeq */
      return 0;
    }
  }

  l_srvcslot_init(slot);
  slot->service = S; /* service is docked default */
  slot->seed_num = l_gen_srvc_seed(M);
  l_service_init(S, slot - stbl->slot_arr, slot->seed_num);
  stbl->num_services += 1;
  l_logm_2(M->E, "service %svid is created (%d)", lx(S->srvc_id), ld(stbl->num_services));
  return slot;
}

static l_service_callback*
l_master_load_service_module(const void* module)
{
  L_UNUSED(module);
  return 0;
#if 0
  l_service_callback* cb = 0;
  l_dynhdl hdl = l_empty_dynlib();

  hdl = l_dynhdl_open2(l_literal_strn(LNLYLIB_CLIB_DIR), module_name); /* TODO: 1. do module cache? 2. multiple service can exist in one module */
  if (l_dynhdl_is_empty(&hdl)) {
    l_loge_1(LNUL, "open library %strn failed", lstrn(&module_name));
    return 0;
  }

/*
  cb.service_proc = (void (*)(lnlylib_env*))l_dynhdl_sym2(&hdl, module_name, l_literal_strn("service_proc"));
  if (cb.service_proc == 0) {
    l_loge_1("load %strn_service_proc failed", l_strn(&module_name));
    return 0;
  }

  cb.service_on_create = (void* (*)(lnlylib_env*))l_dynhdl_sym2(&hdl, module_name, l_literal_strn("service_on_create"));
  cb.service_on_destroy = (void (*)(lnlylib_env*))l_dynhdl_sym2(&hdl, module_name, l_literal_strn("service_on_destroy"));
*/

  cb = (l_service_callback*)l_dynhdl_sym2(&hdl, module_name, l_literal_strn("callback"));
  return cb;
#endif
}

static l_service*
l_master_create_service(l_master* M, l_medit svid, l_service_create_para para)
{
  l_service* S = 0;
  l_srvcslot* slot = 0;
  l_service_callback* cb = 0;
  l_filehdl ioev_hdl = L_EMPTY_HDL;
  l_ushort flags = 0;
  l_ushort events = 0;

  if (para.module) {
    cb = l_master_load_service_module(para.module);
  } else {
    cb = para.cb;
  }

  if (cb == 0) {
    l_loge_s(M->E, "service create fail due to empty cb");
    return 0;
  }

  if (para.enable) {
    if (l_filehdl_is_empty(&para.hdl)) {
      l_sockaddr sa;
      if (!para.ip || !para.port) {
        l_loge_s(M->E, "service create fail due to empty ip/port");
        return 0;
      }
      if (!l_sockaddr_init(&sa, l_strn_c(para.ip), para.port)) {
        l_loge_s(M->E, "service create fail due to invalid address");
        return 0;
      }
      if (para.listen) {
        l_socket sock;
        sock = l_socket_tcp_listen(&sa, 0);
        if (l_socket_is_empty(&sock)) {
          l_loge_s(M->E, "service create fail due to listen fail");
          return 0;
        }
        ioev_hdl = sock;
        flags |= L_SRVC_FLAG_SOCK_LISTEN;
        events |= L_IO_EVENT_READ | L_IO_EVENT_ERR;
      } else {
        l_socket sock;
        l_bool done = false;
        sock = l_socket_tcp_connect(&sa, &done);
        if (l_socket_is_empty(&sock)) {
          l_loge_s(M->E, "service create fail due to connect fail");
          return 0;
        }
        ioev_hdl = sock;
        flags |= L_SRVC_FLAG_SOCK_CONNECT;
        if (!done) flags |= L_SRVC_FLAG_SOCK_PENDING;
        events |= L_IO_EVENT_RDWR | L_IO_EVENT_ERR | L_IO_EVENT_HUP | L_IO_EVENT_RDH;
      }
    } else {
      ioev_hdl = para.hdl;
    }
  }

  if (svid > 0) {
    l_srvctable* stbl = M->stbl;

    if (svid >= L_SRVC_MIN_USER_SVID || svid >= stbl->capacity) {
      l_loge_1(M->E, "invalid reserved service id %d", ld(svid));
      return 0;
    }

    slot = stbl->slot_arr + svid;
    if (slot->service || (slot->flags & L_SERVICE_ALIVE)) {
      l_loge_1(M->E, "reserved service %d already created", ld(svid));
      return slot->service;
    }

    slot = l_master_alloc_service(M, svid);
  } else {
    slot = l_master_alloc_service(M, 0);
  }

  if (slot == 0) {
    l_loge_s(M->E, "service create fail due to slot alloc");
    return 0;
  }

  S = slot->service;
  S->srvc_cb = cb;
  S->srvc_ud = para.svud;
  S->ioev_hdl = ioev_hdl;

  slot->flags |= L_SERVICE_ALIVE | flags;
  slot->events |= events;

  if (l_filehdl_nt_empty(&ioev_hdl)) { /* add this hdl to receive events */
    if (M->evmgr == 0) {
      M->evmgr = &M->ioevent_mgr;
      l_ioevmgr_init(M->evmgr);
    }
    l_ioevmgr_add(M->evmgr, ioev_hdl, S->srvc_id, slot->events);
    slot->events = 0; /* clear and prepare to receive incoming events */
  }

  return S;
}

L_EXTERN void
l_create_service(lnlylib_env* E, l_service_create_para para, void* svud)
{
  l_service_create_req req = {para, 0};
  req.para.svud = svud;
  l_send_message_to_master(E, L_MSG_SERVICE_CREATE_REQ, &req, sizeof(l_service_create_req));
}

L_EXTERN void
l_stop_dest_service(lnlylib_env* E, l_ulong svid)
{
  l_service_stop_req req = {svid};
  l_send_message_to_master(E, L_MSG_SERVICE_STOP_REQ, &req, sizeof(l_service_stop_req));
}

L_EXTERN void
l_stop_service(lnlylib_env* E)
{
  E->S->srvc_flags |= L_SRVC_FLAG_SERVICE_STOP;
}

static void
l_master_stop_service(l_master* M, l_ulong srvc_id)
{
  l_srvctable* stbl = M->stbl;
  l_srvcslot* srvc_slot = 0;
  l_medit svid = l_service_get_slot_index(srvc_id);

  srvc_slot = stbl->slot_arr + svid;
  if (svid >= stbl->capacity || (srvc_slot->flags & L_SERVICE_ALIVE) == 0) {
    l_loge_1(M->E, "try to stop invalid service %d", ld(svid));
  } else {
    l_message* on_destroy_msg = 0;
    on_destroy_msg = l_master_create_message(M, srvc_id, L_MSG_SERVICE_ON_DESTROY, 0, 0);
    l_master_insert_message(M, on_destroy_msg);
  }
}

static void
l_master_destroy_service(l_master* M, l_ulong srvc_id)
{
  l_srvctable* stbl = M->stbl;
  l_srvcslot* srvc_slot = 0;
  l_service* S = 0;
  l_message* srvc_msg = 0;
  l_medit svid = l_service_get_slot_index(srvc_id);

  srvc_slot = stbl->slot_arr + svid;
  if (svid >= stbl->capacity || (srvc_slot->flags & L_SERVICE_ALIVE) == 0) {
    l_loge_1(M->E, "try to destroy invalid service %d", ld(svid));
  } else {
    l_assert(M->E, srvc_slot->service); /* the service must docked when destroy */

    S = srvc_slot->service;
    srvc_slot->service = 0;
    srvc_slot->flags &= (~L_SERVICE_ALIVE);

    if (l_squeue_nt_empty(&S->srvc_msgq)) {
      l_loge_1(M->E, "service %d destroyed with unhandled messages", ld(svid));
      while ((srvc_msg = (l_message*)l_squeue_pop(&S->srvc_msgq))) {
        l_message_free_data(M->E, srvc_msg);
        l_squeue_push(M->mast_frmq, &srvc_msg->node);
      }
    }

    l_service_free_co(S);

    if (l_filehdl_nt_empty(&S->ioev_hdl)) {
      l_ioevmgr_del(M->evmgr, S->ioev_hdl);
      l_socket_close(&S->ioev_hdl);
    }

    /* free the service and slot */
    if ((S->srvc_flags & L_DOCKED_SERVICE_IN_TEMPQ) == 0) {
      l_master_free_service_slot(M, srvc_slot, S);
      S->srvc_flags |= L_DOCKED_SERVICE_IN_TEMPQ; /* service already insert to freeq, forbidden insert to tempq again */
    }
  }
}

/** time and timer handling **/

L_EXTERN l_long
l_time_msec(lnlylib_env* E)
{
  l_long time_ms = 0;
  l_timestamp* tm = E->stamp;
  l_rwlock_rdlock(tm->tmlk);
  time_ms = tm->time_ms;
  l_rwlock_unlock(tm->tmlk);
  return time_ms;
}

L_EXTERN const l_byte*
l_time_strc(lnlylib_env* E)
{
  l_timestamp* tm = E->stamp;
  l_rwlock_rdlock(tm->tmlk);
  E->tmstr = tm->tmstr;
  l_rwlock_unlock(tm->tmlk);
  return l_strbuf_strc(l_sbuf32_p(&E->tmstr));
}

static void
l_master_update_time(l_master* M)
{
  l_long mono_time = l_mono_time_ms();
  l_timestamp* tm = M->stamp;
  l_long time_ms = 0;
  l_sbuf32 tmstr;

  if (mono_time > tm->base_mono_time) {
    time_ms = tm->base_syst_time + mono_time - tm->base_mono_time;
    tmstr = l_timestamp_gen_str(time_ms);

    tm->mast_time_ms = time_ms;

    l_rwlock_wrlock(tm->tmlk);
    tm->time_ms = time_ms;
    tm->tmstr = tmstr;
    l_rwlock_unlock(tm->tmlk);
  }
}

static l_umedit
l_gen_timer_seed(l_timertable* ttbl)
{
  l_umedit seed = ++ttbl->timer_seed;
  if (seed <= L_TIMER_MIN_VALID_TMID) {
    ttbl->timer_seed = L_TIMER_MIN_VALID_TMID;
    return ++ttbl->timer_seed;
  } else {
    return seed;
  }
}

static l_timer_node*
l_timertable_alloc_node(l_timertable* ttbl)
{
  l_timer_node* node = 0;

  node = (l_timer_node*)l_squeue_pop(&ttbl->free_timers);
  if (node != 0) {
    node->timer.uniid = (((l_ulong)(node - ttbl->timer_arr)) << 32) | l_gen_timer_seed(ttbl);
    node->left_msecs = 0;
    ttbl->num_timers += 1;
    return node;
  }

  /* TODO: do dynamic expending */
  return node;
}

static void
l_timertable_free_node(l_timertable* ttbl, l_timer_node* node)
{
  node->timer.uniid = L_TIMER_NOT_ACTIVE_ID;
  l_squeue_push(&ttbl->free_timers, &node->node);
  ttbl->num_timers -= 1;
}

L_EXTERN void
l_create_repeated_timer(lnlylib_env* E, l_uint tmud, l_long ms, void (*func)(lnlylib_env*, void*), void* parm, l_long times)
{
  if (func == 0) {
    l_loge_s(E, "timer func is empty");
  } else {
    l_timer_create_req req;
    req.diff_ms = ms;
    req.svid = E->S->srvc_id;
    req.tmud = tmud;
    req.times = times;
    req.count = 0;
    req.func = func;
    req.parm = parm;
    l_send_message_to_master(E, L_MSG_TIMER_CREATE_REQ, &req, sizeof(l_timer_create_req));
  }
}

L_EXTERN void
l_create_timer(lnlylib_env* E, l_uint tmud, l_long ms, void (*func)(lnlylib_env*, void*), void* parm)
{
  l_create_repeated_timer(E, tmud, ms, func, parm, 1);
}

L_EXTERN void
l_create_notify_timer(lnlylib_env* E, l_uint tmud, l_long ms, l_long times)
{
  l_timer_create_req req;
  req.diff_ms = ms;
  req.svid = E->S->srvc_id;
  req.tmud = tmud;
  req.times = times;
  req.count = 0;
  req.func = 0;
  req.parm = 0;
  l_send_message_to_master(E, L_MSG_TIMER_CREATE_REQ, &req, sizeof(l_timer_create_req));
}

L_EXTERN void
l_cancel_timer(lnlylib_env* E, l_timer* timer)
{
  l_timer_cancel_req req = {*timer};
  l_send_message_to_master(E, L_MSG_TIMER_CANCEL_REQ, &req, sizeof(l_timer_cancel_req));
}

static void
l_master_fire_timer_immediately(l_master* M, l_timer_create_req* req, l_ulong timer_uniid)
{
  if (req->func) {
    l_timer_func_call msg;
    msg.func = req->func;
    msg.parm = req->parm;
    l_master_send_message_to_service(M, req->svid, L_MSG_TIMER_FUNC_CALL, &msg, sizeof(l_timer_func_call));
  } else {
    l_timer_notify_ind ind;
    ind.stamp = M->stamp->mast_time_ms;
    ind.count = ++req->count;
    ind.tmud = req->tmud;
    ind.timer.uniid = timer_uniid;
    l_master_send_message_to_service(M, req->svid, L_MSG_TIMER_NOTIFY_IND, &ind, sizeof(l_timer_notify_ind));
  }
}

static l_medit
l_get_timer_index(l_timer timer)
{
  return (l_medit)(((timer.uniid << 1) >> 1) >> 32);
}

static void
l_master_cancel_timer(l_master* M, l_timer timer)
{
  l_timertable* ttbl = M->ttbl;
  l_timer_node* node = 0;
  l_medit index = 0;

  index = l_get_timer_index(timer);
  node = ttbl->timer_arr + index;

  if (index < ttbl->capacity && node->timer.uniid == timer.uniid) {
    node->timer.uniid |= L_TIMER_CANCELED_FLAG;
  }
}

static l_bool
l_master_add_timer(l_master* M, l_timer_node* timer)
{
  l_timertable* ttbl = M->ttbl;
  l_long diff = timer->expire_time - M->stamp->mast_time_ms;
  l_squeue* tmrq = 0;
  l_long index = 0;

  if (diff <= L_MAX_TIMEQ_SIZE) {
    if (diff < 1) diff = 1;
    tmrq = ttbl->msec_level->msecq;
    index = (diff - 1 + (ttbl->msec_level->cur_msec - tmrq)) & (L_MAX_TIMEQ_SIZE - 1);
    l_squeue_push(tmrq + index, &timer->node);
    return true;
  }

  diff /= 100; /* now diff's unit is 100ms */
  if (diff <= L_MAX_TIMEQ_SIZE) {
    tmrq = ttbl->hms_level->hmsq;
    index = (diff - 1 + (ttbl->hms_level->cur_hms - tmrq)) & (L_MAX_TIMEQ_SIZE - 1);
    l_squeue_push(tmrq + index, &timer->node);
    return true;
  }

  diff /= 10; /* now diff is secs */
  if (diff <= L_MAX_TIMEQ_SIZE) { /* <= 4096s */
    tmrq = ttbl->sec_level->secq;
    index = (diff - 1 + (ttbl->sec_level->cur_sec - tmrq)) & (L_MAX_TIMEQ_SIZE - 1);
    l_squeue_push(tmrq + index, &timer->node);
    return true;
  }

  l_timertable_free_node(M->ttbl, timer);

  l_loge_1(M->E, "timer expire time too long (> %d-hour)", ld(diff/3600));
  return false;
}

static void
l_master_start_timer(l_master* M, l_timer_node* timer, l_bool first_creation)
{
  l_bool succ = false;
  l_long cur_ms = M->stamp->mast_time_ms;
  l_long time_diff = timer->expire_time - cur_ms;
  l_long max_ms = L_MAX_TIMEQ_SIZE * 1000;

  if (time_diff > max_ms) { /* > 4096s */
    timer->expire_time = cur_ms + max_ms;
    timer->left_msecs = time_diff - max_ms;
  }

  succ = l_master_add_timer(M, timer);
  if (!first_creation) {
    return;
  }

  if (succ) {
    l_timer_create_rsp rsp;
    rsp.tmud = timer->data.tmud;
    rsp.timer = timer->timer;
    l_master_send_message_to_service(M, timer->data.svid, L_MSG_TIMER_CREATE_RSP, &rsp, sizeof(l_timer_create_rsp));
  } else {
    l_timer_create_rsp rsp;
    rsp.tmud = timer->data.tmud;
    rsp.timer.uniid = L_TIMER_ADD_FAILED_ID;
    l_master_send_message_to_service(M, timer->data.svid, L_MSG_TIMER_CREATE_RSP, &rsp, sizeof(l_timer_create_rsp));
  }
}

static void
l_master_repeat_timer(l_master* M, l_timer_node* timer)
{
  timer->expire_time += timer->data.diff_ms;
  l_master_start_timer(M, timer, false);
}

static void
l_master_create_timer(l_master* M, l_timer_create_req* req)
{
  if (req->diff_ms <= 0) { /* fire immediately */
    l_timer_create_rsp rsp;
    rsp.tmud = req->tmud;
    rsp.timer.uniid = L_TIMER_IMMED_FIRED_ID;
    l_master_send_message_to_service(M, req->svid, L_MSG_TIMER_CREATE_RSP, &rsp, sizeof(l_timer_create_rsp));
    l_master_fire_timer_immediately(M, req, L_TIMER_IMMED_FIRED_ID);
  } else {
    l_timer_node* node = 0;
    node = l_timertable_alloc_node(M->ttbl);
    node->expire_time = M->stamp->mast_time_ms + req->diff_ms;
    node->data = *req;
    l_master_start_timer(M, node, true);
  }
}

static void
l_master_fire_timer(l_master* M, l_timer_node* timer)
{
  l_long max_ms = L_MAX_TIMEQ_SIZE * 1000;

  if (timer->timer.uniid & L_TIMER_CANCELED_FLAG) {
    l_timertable_free_node(M->ttbl, timer);
    return;
  }

  if (timer->left_msecs > max_ms) { /* > 4096s */
    timer->expire_time += max_ms;
    timer->left_msecs -= max_ms;
    l_master_add_timer(M, timer);
  } else if (timer->left_msecs > 0) { /* left time less than 4096s */
    timer->expire_time += timer->left_msecs;
    timer->left_msecs = 0;
    l_master_add_timer(M, timer);
  } else { /* no time left, fire timer */
    l_timer_create_req* req = &timer->data;
    l_master_fire_timer_immediately(M, req, timer->timer.uniid);
    if (req->times < 0) { /* repeat forever until cancel */
      l_master_repeat_timer(M, timer);
    } else if (req->times <= 1) { /* no need to repeat */
      l_timertable_free_node(M->ttbl, timer);
    } else {
      req->times -= 1;
      l_master_repeat_timer(M, timer);
    }
  }
}

static l_squeue*
l_check_current_timer_queues(l_squeue* start, l_int qcnt, l_squeue* cur, l_int expired_cnt, l_squeue* fired_q)
{
  l_int cur_pos = cur - start;
  l_int expired_pos = (expired_cnt + cur_pos) & (qcnt - 1);
  if (expired_pos > cur_pos) {
    l_squeue* qend = start + expired_pos;
    for (; cur < qend; ++cur) {
      l_squeue_push_queue(fired_q, cur);
    }
  } else {
    l_squeue* qend = start + qcnt;
    for (; cur < qend; ++cur) {
      l_squeue_push_queue(fired_q, cur);
    }
    cur = start;
    qend = start + expired_pos;
    for (; cur < qend; ++cur) {
      l_squeue_push_queue(fired_q, cur);
    }
  }
  return start + expired_pos;
}

static l_squeue*
l_expire_all_current_queues(l_squeue* start, l_int qcnt, l_squeue* fired_q)
{
  l_squeue* cur = start;
  l_squeue* qend = start + qcnt;
  for (; cur < qend; ++cur) {
    l_squeue_push_queue(fired_q, cur);
  }
  return start;
}

static void
l_check_msec_level_timers(l_timer_msec_level* msec_level, l_long cur_ms, l_squeue* fired_q)
{
  l_long diff_ms = cur_ms - msec_level->base_ms;

  if (diff_ms <= 0) {
    return;
  }

  msec_level->base_ms = cur_ms;

  if (diff_ms <= L_MAX_TIMEQ_SIZE) {
    msec_level->cur_msec = l_check_current_timer_queues(msec_level->msecq, L_MAX_TIMEQ_SIZE, msec_level->cur_msec, diff_ms, fired_q);
  } else { /* all timers in msec level are expired */
    msec_level->cur_msec = l_expire_all_current_queues(msec_level->msecq, L_MAX_TIMEQ_SIZE, fired_q);
  }
}

static void
l_check_hms_level_timers(l_timer_hms_level* hms_level, l_long cur_ms, l_squeue* fired_q)
{
  l_long diff_hms = (cur_ms / 100) - hms_level->base_hms;

  if (diff_hms <= 0) {
    return;
  }

  hms_level->base_hms = cur_ms / 100;

  if (diff_hms <= L_MAX_TIMEQ_SIZE) {
    hms_level->cur_hms = l_check_current_timer_queues(hms_level->hmsq, L_MAX_TIMEQ_SIZE, hms_level->cur_hms, diff_hms, fired_q);
  } else { /* all timers in hms level are expired */
    hms_level->cur_hms = l_expire_all_current_queues(hms_level->hmsq, L_MAX_TIMEQ_SIZE, fired_q);
  }
}

static void
l_check_sec_level_timers(l_timer_sec_level* sec_level, l_long cur_ms, l_squeue* fired_q)
{
  l_long diff_sec = (cur_ms / 1000) - sec_level->base_sec;

  if (diff_sec <= 0) {
    return;
  }

  sec_level->base_sec = cur_ms / 1000;

  if (diff_sec <= L_MAX_TIMEQ_SIZE) {
    sec_level->cur_sec = l_check_current_timer_queues(sec_level->secq, L_MAX_TIMEQ_SIZE, sec_level->cur_sec, diff_sec, fired_q);
  } else {
    sec_level->cur_sec = l_expire_all_current_queues(sec_level->secq, L_MAX_TIMEQ_SIZE, fired_q);
    l_loge_1(LNUL, "there is long time didn't check timers (> %d-hour)", ld(diff_sec/3600));
  }
}

static void
l_master_check_timers(l_master* M)
{
  l_timertable* ttbl = M->ttbl;
  l_long current_ms = M->stamp->mast_time_ms;
  l_timer_node* timer = 0;
  l_squeue fired_q;

  if (current_ms <= ttbl->msec_level->base_ms) {
    return;
  }

  l_squeue_init(&fired_q);

  l_check_msec_level_timers(ttbl->msec_level, current_ms, &fired_q);
  l_check_hms_level_timers(ttbl->hms_level, current_ms, &fired_q);
  l_check_sec_level_timers(ttbl->sec_level, current_ms, &fired_q);

  while ((timer = (l_timer_node*)l_squeue_pop(&fired_q))) {
    l_master_fire_timer(M, timer);
  }
}

/** socket service **/

typedef struct {
  l_service_callback* inconn_cb;
  l_dqueue connq;
  l_dqueue freeq;
  l_umedit conns;
  l_ushort local_port;
  l_ipstr local_ip;
} l_socket_listen_svud;

static void* l_socket_listen_service_on_create(lnlylib_env* E);
static void l_socket_listen_service_on_destroy(lnlylib_env* E);
static void l_socket_listen_service_proc(lnlylib_env* E);

static l_service_define
l_socket_listen_service = {
  l_socket_listen_service_on_create,
  l_socket_listen_service_on_destroy,
  l_socket_listen_service_proc
};

L_EXTERN l_service_create_para
l_listen_service_para(const void* local_ip, l_ushort local_port, l_service_callback* conn_cb)
{
  l_service_create_para para = L_LISTEN_SERVICE(local_ip, local_port, &l_socket_listen_service);
  para.svud = conn_cb;
  return para;
}

L_EXTERN void
l_stop_listen_service(lnlylib_env* E)
{
  l_send_message_to(E, l_current_svfr(E), 0, L_MSG_STOP_SKLS_CMD, 0, 0);
}

#if 0
static void
l_create_connect_service(lnlylib_env* E, const void* remote_ip, l_ushort remote_port, l_uint mgud)
{
  l_socket_data_svud* data_svud = 0;
}
#endif

typedef struct {
  l_ushort local_port;
  l_ushort remote_port;
  const l_ipstr* local_ip;
  const l_ipstr* remote_ip;
} l_socket_info_data;

typedef struct {
  l_linknode node;
  l_squeue wrmq;
  l_squeue rdmq;
  l_bool outconn;
  l_ulong user_svid;
  l_socket_info_data info;
  l_service_callback* inconn_cb;
  l_ipstr rmt_ip;
} l_socket_data_svud;

static void
l_socket_data_svud_init(l_socket_data_svud* s)
{
  L_UNUSED(s);
}

static void
l_socket_data_svud_free(l_socket_data_svud* s)
{
  L_UNUSED(s);
}

static void* l_socket_data_service_on_create(lnlylib_env* E);
static void l_socket_data_service_on_destroy(lnlylib_env* E);
static void l_socket_data_service_proc(lnlylib_env* E);

static l_service_define
l_socket_data_service = {
  l_socket_data_service_on_create,
  l_socket_data_service_on_destroy,
  l_socket_data_service_proc
};

static void*
l_socket_listen_service_on_create(lnlylib_env* E)
{
  l_socket_listen_svud* listen = 0;
  l_sockaddr local;

  listen = L_MALLOC_TYPE(E, l_socket_listen_svud);

  listen->inconn_cb = (l_service_callback*)l_current_svud(E);
  listen->conns = 0;
  l_dqueue_init(&listen->connq);
  l_dqueue_init(&listen->freeq);

  local = l_sockaddr_local(l_current_evhd(E));
  listen->local_port = l_sockaddr_port(&local);
  listen->local_ip = l_sockaddr_getip(&local);

  l_set_current_svud(E, listen);
  return 0;
}

static void
l_socket_listen_service_on_destroy(lnlylib_env* E)
{
  l_socket_listen_svud* data = 0;
  data = (l_socket_listen_svud*)l_current_svud(E);
  l_mfree(E, data);
}

static void
l_socket_listen_service_accept_conn(void* ud, l_socketconn* conn)
{
  lnlylib_env* E = (lnlylib_env*)ud;
  l_sockaddr* remote = 0;
  l_ipstr rmt_ip;
  l_ushort rmt_port;
  int accept = true;

  remote = &conn->remote;
  rmt_port = l_sockaddr_port(remote);
  rmt_ip = l_sockaddr_getip(remote);

  /* TODO: do remote address filter here */

  if (accept) {
    l_socket_listen_svud* listen = 0;
    l_socket_data_svud* data_svud = 0;

    listen = (l_socket_listen_svud*)l_current_svud(E);
    data_svud = (l_socket_data_svud*)l_dqueue_pop(&listen->freeq);
    if (data_svud == 0) {
      data_svud = L_MALLOC_TYPE(E, l_socket_data_svud);
    }

    l_zero_n(data_svud, sizeof(l_socket_data_svud));

    data_svud->inconn_cb = listen->inconn_cb;
    data_svud->info.local_port = listen->local_port;
    data_svud->info.local_ip = &listen->local_ip;
    data_svud->info.remote_port = rmt_port;
    data_svud->info.remote_ip = &data_svud->rmt_ip;
    data_svud->rmt_ip = rmt_ip;

    l_create_service(E, L_USEHDL_SERVICE(conn->sock, &l_socket_data_service), data_svud);
  }
}

typedef struct {
  l_socket_data_svud* data_svud;
} l_sock_disc_cmd;

typedef struct {
  l_ulong sock_srvc;
} l_sock_ready_ntf;

static void
l_socket_listen_service_proc(lnlylib_env* E)
{
  l_service* S = l_current_service(E);
  l_message* MSG = l_current_message(E);
  l_socket_listen_svud* listen = 0;

  listen = (l_socket_listen_svud*)l_current_svud(E);

  switch (l_mgid(MSG)) {
  case L_MSG_SOCK_ACCEPT_IND: /* from master */
    l_socket_accept(l_evhd(S), l_socket_listen_service_accept_conn, E);
    break;
  case L_MSG_SUBSRVC_CREATE_RSP: {
    l_subsrvc_create_rsp* rsp = 0;
    l_socket_data_svud* data_svud = 0;
    rsp = (l_subsrvc_create_rsp*)l_mgdt(MSG);
    data_svud = (l_socket_data_svud*)rsp->svud;
    if (rsp->svid) {
      listen->conns += 1;
      l_dqueue_push(&listen->connq, &data_svud->node);

      l_send_message_to(E, rsp->svid, 0, L_MSG_SOCK_CONN_NTF, 0, 0);
    } else {
      l_dqueue_push(&listen->freeq, &data_svud->node);
    }}
    break;
  case L_MSG_SOCK_DISC_NTF:
  case L_MSG_SOCK_ERROR_NTF:
    break;
  case L_MSG_SOCK_DISC_CMD: { /* from socket data service */
    l_socket_data_svud* data_svud = 0;
    data_svud = ((l_sock_disc_cmd*)l_mgdt(MSG))->data_svud;
    l_socket_data_svud_free(data_svud);
    listen->conns -= 1;
    l_linknode_remove(&data_svud->node);
    l_dqueue_push(&listen->freeq, &data_svud->node);
    l_stop_dest_service(E, l_mgfr(MSG));
    }
    break;
  case L_MSG_SERVICE_EXIT_REQ:
    break;
  case L_MSG_SERVICE_RESTART_REQ:
    break;
  default:
    break;
  }
}

/** socket data service **/

static void*
l_socket_data_service_on_create(lnlylib_env* E)
{
  L_UNUSED(E); /* nothing to do */
  return 0;
}

static void
l_socket_data_service_on_destroy(lnlylib_env* E)
{
  l_socket_data_svud* data_svud = 0;
  data_svud = (l_socket_data_svud*)l_current_svud(E);
  if (data_svud->outconn) {
    l_socket_data_svud_free(data_svud);
    l_mfree(E, data_svud);
  }
}

static void
l_socket_data_service_proc(lnlylib_env* E)
{
  /** related messages **
  REQ is sent from client to server and requires server RSP. CMD is sent from client to server, no response.
  IND is sent from server to client and requires client CNF, NTF is sent from server to client, no response.
  (1) socket user service
  [RX] L_MSG_SOCK_READY_NTF (all these are freom socket data txrx service)
       L_MSG_SOCK_READ_RSP
       L_MSG_SOCK_WRITE_RSP
       L_MSG_SOCK_BAD_STATE_NTF
       L_MSG_SOCK_RECOVER_RSP
  [TX] L_MSG_SOCK_DATA_RX_REQ     <- l_socket_service_read(sock_srvc)
       L_MSG_SOCK_DATA_TX_REQ     <- l_socket_service_write(sock_svrc)
       L_MSG_SOCK_RECOVER_REQ     <- l_socket_service_recover(sock_srvc)
       L_MSG_SOCK_CLOSE_CMD       <- l_socket_service_close(sock_srvc)
       L_MSG_STOP_SKLS_CMD        <- l_stop_listen_server(E)
  (2) socket data txrx service
  [RX] L_MSG_SOCK_CONN_NTF (from listen server after incoming connection accepted)
       L_MSG_SOCK_CONN_RSP (from master after initiated connection established)
       L_MSG_SOCK_READY_TX (these are common socket messages from master)
       L_MSG_SOCK_READY_RX
       L_MSG_SOCK_DISC_NTF
       L_MSG_SOCK_ERROR_NTF
       L_MSG_SOCK_DATA_TX_REQ (these are from user service)
       L_MSG_SOCK_DATA_RX_REQ
       L_MSG_SOCK_RECOVER_REQ
       L_MSG_SOCK_CLOSE_CMD
       L_MSG_STOP_SKLS_CMD
  [TX] L_MSG_SOCK_DISC_CMD (these are sent to listen server)
       L_MSG_STOP_SKLS_CMD
       L_MSG_SOCK_READY_NTF (these are sent to user service)
       L_MSG_SOCK_BAD_STATE_NTF
       L_MSG_SOCK_WRITE_RSP
       L_MSG_SOCK_READ_RSP
       L_MSG_SOCK_RECOVER_RSP
  (3) socket listen server service
  [RX] L_MSG_SOCK_ACCEPT_IND (from master when a connection incoming)
       L_MSG_SOCK_ERROR_NTF (from master when socket error)
  ----
  flow for incoming socket connection
       1. user pass inconn cb to service (3) via svud on create
       2. service (3) get inconn cb and replace svud to its real svud on create
       3. when (3) received L_MSG_SOCK_ACCEPT_IND, it creates a service (2) for each connection incoming
       4. when the service created success, (3) send L_MSG_SOCK_CONN_NTF to it
       5. when service (2) received L_MSG_SOCK_CONN_NTF, it create a service (1) to handle user logic (i.e. inconn cb)
       6. if the user service created success, (2) send L_MSG_SOCK_READY_NTF to it
       7. if created failed, (2) is no need to exist, send L_MSG_SOCK_DISC_CMD to (3) to release connection and destroy itself
       8. now, service (1) can tx/rx socket data via socket service (2), and if needed, (1) can also create outconn socket services
  **/

  l_message* MSG = E->MSG;
  l_service* S = E->S;
  l_socket_data_svud* svud = 0;
  l_message* msg = 0;

  svud = (l_socket_data_svud*)E->svud;

  switch (MSG->mssg_id) {
  case L_MSG_SOCK_CONN_NTF: { /* from listen service, a incoming connection established */
    l_socket_data_svud* svud = 0;
    svud = (l_socket_data_svud*)l_current_svud(E);
    l_create_service(E, L_SERVICE(svud->inconn_cb), &svud->info);
    }
    break;
  case L_MSG_SUBSRVC_CREATE_RSP: {
    l_subsrvc_create_rsp* rsp = 0;
    rsp = (l_subsrvc_create_rsp*)l_mgdt(MSG);
    if (rsp->svid) {
      l_sock_ready_ntf ntf;
      svud->user_svid = rsp->svid;
      ntf.sock_srvc = l_svid(S);
      l_send_message_to(E, svud->user_svid, 0, L_MSG_SOCK_READY_NTF, &ntf, sizeof(l_sock_ready_ntf));
    } else {
      l_sock_disc_cmd cmd;
      cmd.data_svud = svud;
      l_send_message_to(E, l_svfr(S), 0, L_MSG_SOCK_DISC_CMD, &cmd, sizeof(l_sock_disc_cmd));
    }}
    break;
  case L_MSG_SOCK_CONN_RSP: { /* from master, initiated connection established */
    l_socket_data_svud* data_svud = 0;
    l_sock_ready_ntf ntf;

    /* TODO - complete the connection first */

    data_svud = L_MALLOC_TYPE(E, l_socket_data_svud);
    l_zero_n(data_svud, sizeof(l_socket_data_svud));
#if 0
    data_svud->info.local_port = listen->local_port;
    data_svud->info.local_ip = listen->local_ip;
    data_svud->info.remote_port = rmt_port;
    data_svud->info.remote_ip = data_svud->rmt_ip;
    data_svud->rmt_ip = rmt_ip;
#endif

    data_svud->user_svid = l_svfr(S);
    data_svud->outconn = true;
    ntf.sock_srvc = l_svid(S);
    l_send_message_to(E, data_svud->user_svid, 0, L_MSG_SOCK_READY_NTF, &ntf, sizeof(l_sock_ready_ntf));
    }
    break;
  /* common socket messages from master */
  case L_MSG_SOCK_READY_RX:
#if 0
    if ((msg = (l_message*)l_squeue_top(&svud->rdmq))) {
      data = msg->mssg_data + msg->mgid_cust;
      size = msg->data_size - msg->mgid_cust;
      done = l_data_read(S->ioev_hdl, data, size);
      if (done == size) {
        l_squeue_pop(&svud->wrmq);
        /* TODO: send L_MSG_SOCK_READ_RSP to user service */
      } else {
        msg->mgid_cust += done;
      }
    }
#endif
    break;
  case L_MSG_SOCK_READY_TX:
#if 0
    if ((msg = (l_message*)l_squeue_top(&svud->wrmq))) {
      data = msg->mssg_data + msg->mgid_cust;
      size = msg->data_size - msg->mgid_cust;
      done = l_data_write(S->ioev_hdl, data, size);
      if (done == size) {
        l_squeue_pop(&svud->wrmq);
        l_message_reset(E, msg, svud->user_svid, msg->dest_mgud, msg->session, L_MSG_SOCK_WRITE_RSP, 0, 0, 0);
        msg->extra.a = ++svud->wrid;
        l_send_message_impl(E, msg);
      } else {
        msg->mgid_cust += done;
      }
    }
#endif
    break;
  case L_MSG_SOCK_DISC_NTF:
  case L_MSG_SOCK_ERROR_NTF:
    l_send_message_to(E, svud->user_svid, 0, L_MSG_SOCK_BAD_STATE_NTF, 0, 0);
    break;
  /* messages from user service */
  case L_MSG_SOCK_DATA_RX_REQ:
    if (msg->data_size && msg->mssg_data) {
      msg->mssg_flags |= L_MSSG_FLAG_DONT_AUTO_FREE;
      // msg->mgid_cust = 0; /* use mgid_cust to record how many data alrady read */
      l_squeue_push(&svud->rdmq, &msg->node);
    }
    break;
  case L_MSG_SOCK_DATA_TX_REQ:
    if (msg->data_size && msg->mssg_data) {
      msg->mssg_flags |= L_MSSG_FLAG_DONT_AUTO_FREE;
      // msg->mgid_cust = 0; /* use mgid_cust to record how many data already written */
      l_squeue_push(&svud->wrmq, &msg->node);
    }
    break;
  case L_MSG_SOCK_RECOVER_REQ:
    break;
  case L_MSG_SOCK_CLOSE_CMD: {
    l_ulong from_id = l_svfr(S);
    if (from_id == 0) {
      l_stop_service(E); /* stop and destroy self, socket will auto closed when destroy */
    } else {
      l_sock_disc_cmd cmd;
      cmd.data_svud = svud; /* this service is destroyed after this cmd */
      l_send_message_to(E, l_svfr(S), 0, L_MSG_SOCK_DISC_CMD, &cmd, sizeof(l_sock_disc_cmd));
    }}
    break;
  case L_MSG_STOP_SKLS_CMD:
    if (l_svfr(S)) {
    }
    break;
  default:
    break;
  }
}

/** memory opeartion - <stdlib.h> <string.h> **
void* malloc(size_t size);
void* calloc(size_t num, size_t size);
void* realloc(void* p, size_t size);
void free(void* p);
---
realloc changes the size of the memory block pointed by buffer. It
may move the memory block to a new location (its address is returned
by the function). The content of the memory block is preserved up to
the lesser of the new and old sizes, even if the block is moved to a
new location. ***If the new size is larger, the value of the newly
allocated portion is indeterminate***.
In case of that buffer is a null pointer, the function behaves like malloc,
assigning a new block of size bytes and returning a pointer to its beginning.
If size is zero, the memory previously allocated at buffer is deallocated
as if a call to free was made, and a null pointer is returned. For c99/c11,
the return value depends on the particular library implementation, it may
either be a null pointer or some other location that shall not be dereference.
If the function fails to allocate the requested block of memory, a null
pointer is returned, and the memory block pointed to by buffer is not
deallocated (it is still valid, and with its contents unchanged). **/

static void*
l_rawalloc_func(void* ud, void* p, l_ulong oldsz, l_ulong newsz)
{
  L_UNUSED(ud);
  L_UNUSED(oldsz);
  if (newsz == 0) {
    if (p) free(p);
    return 0;
  } else if (p == 0) {
    return malloc(newsz);
  } else {
    return realloc(p, newsz);
  }
}

l_allocfunc l_alloc_func = l_rawalloc_func;

L_EXTERN l_bool
l_zero_n(void* p, l_ulong size)
{
  if (p == 0 || size == 0) {
    return false;
  }
  if (p == memset(p, 0, size)) {
    return true;
  } else {
    l_loge_1(LNUL, "memset fail %d", ld(size));
    return false;
  }
}

L_EXTERN l_ulong
l_copy_n(void* dest, const void* from, l_ulong size)
{
  if (dest == 0 || from == 0 || size == 0) {
    return 0;
  }
  if (l_strc(dest) + size <= l_strc(from) || l_strc(from) + size <= l_strc(dest)) {
    if (dest == memcpy(dest, from, size)) {
      return size;
    } else {
      l_loge_3(LNUL, "memcpy fail %p <- %p %d", lp(dest), lp(from), ld(size));
      return 0;
    }
  } else {
    if (dest == memmove(dest, from, size)) {
      return size;
    } else {
      l_loge_3(LNUL, "memmove fail %p <- %p %d", lp(dest), lp(from), ld(size));
      return 0;
    }
  }
}

L_EXTERN l_bool
l_strn_equal(const l_strn* a, l_strn b)
{
  if (a->n != b.n) {
    return false;
  } else {
    return strncmp((const char*)a->p, (const char*)b.p, b.n) == 0;
  }
}

L_EXTERN l_bool
l_strn_has(const l_strn* a, l_byte c)
{
  return memchr(a->p, c, a->n) != 0;
}

/** debug and logging **/

static int l_global_loglevel = 3;

L_EXTERN int
l_set_log_level(int n)
{
  int oldval = l_global_loglevel;
  l_global_loglevel = (n >= 0 ? n : 3);
  return oldval;
}

static void
l_finish_logging(lnlylib_env* E)
{
  if (E == 0) {
    E = l_get_lnlylib_env();
  }

  if (E) {
    l_ostream_flush(E->logout);
  }
}

static l_ostream*
l_start_logging(lnlylib_env* E, const l_byte* tag, l_ostream* temp_out)
{
  l_ostream* out = 0;
  l_umedit thridx = 9999;
  l_umedit svid = 0;
  const l_byte* tmstr = 0;

  if (E == 0) {
    E = l_get_lnlylib_env();
  }

  if (E == 0) {
    *temp_out = l_stdout_ostream();
    out = temp_out;
    tmstr = l_strc("0000/00/00 00:00:00.000");
  } else {
    out = E->logout;
    thridx = E->T->thridx;
    svid = E->S ? (l_umedit)(E->S->srvc_id >> 32) : 0;
    tmstr = l_time_strc(E);
  }

  l_ostream_format_4(out, "%s\t%s %4d:%.8zx ", ls(tag), ls(tmstr), ld(thridx), ld(svid));
  return out;
}

static int l_impl_ostream_format_v(l_ostream* os, const void* fmt, l_int n, va_list vl);

L_EXTERN void
l_impl_logger_func(lnlylib_env* E, const void* tag, const void* fmt, ...)
{
  int level = l_strc(tag)[0] - '0';

  if (!fmt || level > l_global_loglevel) {
    return;
  } else {

    int nargs = l_strc(tag)[1];
    l_ostream temp_out;
    l_ostream* out = 0;
    va_list vl;

    out = l_start_logging(E, l_strc(tag) + 2, &temp_out);

    if (nargs == 'n') {
      va_start(vl, fmt);
      nargs = va_arg(vl, l_int);
      l_ostream_format_n(out, fmt, nargs, va_arg(vl, l_value*));
      va_end(vl);
    } else {
      va_start(vl, fmt);
      l_impl_ostream_format_v(out, fmt, nargs - '0', vl);
      va_end(vl);
    }

    l_ostream_write(out, L_NEWLINE, L_NL_SIZE);
  }
}

/** output stream **/

/**
 * [non-zero] pritable charater
 * [00111010] G ~ Z   0x3a
 * [00111011] g ~ z   0x3b
 * [00111101] 0 ~ 9   0x3d
 * [00111110] A ~ F   0x3e
 * [00111111] a ~ f   0x3f
 * [00110000] _       0x30
 * [00100000] -       0x20
 * [0000XX1X] letter                 (ch & 0x02)
 * [0000XX10] upper letter           (ch & 0x03) == 2
 * [0000XX11] lower letter           (ch & 0x03) == 3
 * [0000X1XX] hex digit              (ch & 0x04)
 * [00001XXX] alphanum               (ch & 0x08)
 * [XXX1XXXX] alphanum and _         (ch & 0x10)
 * [XX1XXXXX] alphanum and _ and -   (ch & 0x20)
 */
static const l_byte l_char_table[] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80, 0x80,0x80,0x80,0x80,0x80,0x20,0x80,0x80, 0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d,0x3d, 0x3d,0x3d,0x80,0x80,0x80,0x80,0x80,0x80, /* (20) - (3d) 0 ~ 9 */
  0x80,0x3e,0x3e,0x3e,0x3e,0x3e,0x3e,0x3a, 0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a, 0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a,0x3a, 0x3a,0x3a,0x3a,0x80,0x80,0x80,0x80,0x30, /* (3e,3a) A ~ Z (30) _ */
  0x80,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3b, 0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b, 0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b,0x3b, 0x3b,0x3b,0x3b,0x80,0x80,0x80,0x80,0x00, /* (3f,3b) a ~ z */
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const l_strn l_hex_digit[] = {
  l_literal_strn("0123456789abcdef"),
  l_literal_strn("0123456789ABCDEF")
};

L_EXTERN int
l_is_printable(l_byte c)
{
  return l_char_table[c];
}

L_EXTERN int
l_is_dec_digit(l_byte c)
{
  return l_char_table[c] == 0x3d;
}

L_EXTERN int
l_is_letter(l_byte c)
{
  return l_char_table[c] & 0x02;
}

L_EXTERN int
l_is_upper_letter(l_byte c)
{
  return (l_char_table[c] & 0x03) == 2;
}

L_EXTERN int
l_is_lower_letter(l_byte c)
{
  return (l_char_table[c] & 0x03) == 3;
}

L_EXTERN int
l_is_hex_digit(l_byte c)
{
  return l_char_table[c] & 0x04;
}

L_EXTERN int
l_is_alphanum(l_byte c)
{
  return l_char_table[c] & 0x08;
}

L_EXTERN int
l_is_alphanum_underscore(l_byte c)
{
  return l_char_table[c] & 0x10;
}

L_EXTERN int
l_is_alphanum_underscore_hyphen(l_byte c)
{
  return l_char_table[c] & 0x20;
}

L_EXTERN l_int
l_dec_string_to_int(l_strn s)
{
  l_int times = 1;
  l_int value = 0;
  const l_byte* start = 0;
  const l_byte* end = 0;
  const l_byte* s_end = s.p + s.n;
  int negative = false;

  while (s.p < s_end) {
    if (*s.p >= '0' && *s.p <= '9') break;
    if (*s.p == '-') negative = true;
    ++s.p;
  }

  start = s.p;
  while (s.p < s_end) {
    if (*s.p < '0' || *s.p > '9') break;
    ++s.p;
  }

  end = s.p;
  while (start < end--) {
    value += *end * times;
    times *= 10;
  }

  return (negative ? -value : value);
}

L_EXTERN l_int
l_hex_string_to_int(l_strn s)
{
  l_int times = 1;
  l_int value = 0;
  const l_byte* start = 0;
  const l_byte* end = 0;
  const l_byte* s_end = s.p + s.n;
  int negative = false;

  while (s.p < s_end) {
    if (l_is_hex_digit(*s.p)) break;
    if (*s.p == '-') negative = true;
    ++s.p;
  }

  if (*s.p == '0' && s.p + 1 < s_end && (*(s.p + 1) == 'x' || *(s.p + 1) == 'X')) {
    s.p += 2;
    if (s.p >= s_end || !l_is_hex_digit(*s.p)) {
      return 0;
    }
  }

  start = s.p;
  while (s.p < s_end) {
    if (!l_is_hex_digit(*s.p)) break;
    ++s.p;
  }

  end = s.p;
  while (start < end--) {
    value += *end * times;
    times *= 16;
  }

  return (negative ? -value : value);
}

static l_int
l_ostream_format_fill(l_ostream* os, l_byte* a, l_byte* p, l_umedit flags)
{
  l_byte fill = L_GETF(flags);
  int width = L_GETW(flags);

  if (width > p - a) {
    if (fill == 0) {
      fill = ' ';
    }
    if (flags & L_LEFT) {
      while (width > p - a) {
        *p++ = fill;
      }
    } else {
      l_byte* e = a + width;
      while (p > a) {
        *(--e) = *(--p);
      }
      while (e > a) {
        *(--e) = fill;
      }
      p = a + width;
    }
  }

  return l_ostream_write(os, a, p - a);
}

L_EXTERN l_int
l_ostream_format_strn(l_ostream* os, l_strn s, l_umedit flags)
{
  int width = L_GETW(flags);
  if (l_strn_is_empty(&s)) {
    return 0;
  }
  if (s.n >= width) {
    return l_ostream_write(os, s.p, s.n);
  } else {
    l_byte a[160];
    memcpy(a, s.p, s.n);
    return l_ostream_format_fill(os, a, a + s.n, flags);
  }
}

L_EXTERN l_int
l_ostream_format_c(l_ostream* os, int c, l_umedit flags)
{
  l_byte ch = (l_byte)(c & 0xff);
  if (flags & L_UPPER) {
    if (ch >= 'a' && ch <= 'z') {
      ch -= 32;
    }
  } else if (flags & L_LOWER) {
    if (ch >= 'A' && ch <= 'Z') {
      ch += 32;
    }
  }
  return l_ostream_format_strn(os, l_strn_l(&ch, 1), flags);
}

#define L_HEX_FMT_BFSZ 159

static l_umedit
l_right_most_bit(l_umedit n)
{
  return n & (-n);
}

L_EXTERN l_int
l_ostream_format_u(l_ostream* os, l_ulong u, l_umedit flags)
{
  /* 64-bit unsigned int max value 18446744073709552046 (20 chars) */
  l_byte a[L_HEX_FMT_BFSZ];
  l_byte basechar = 0;
  l_byte* e = a + L_HEX_FMT_BFSZ - 1;
  l_byte* p = e;
  const l_byte* hex = 0;
  l_umedit base = 0;
  l_byte precise = L_GETP(flags);
  l_byte width = L_GETW(flags);
  l_byte fill = L_GETF(flags);

  flags &= L_FORMAT_MASK;
  base = (flags & L_BASE_MASK);

  switch (l_right_most_bit(base)) {
  case 0:
    *--p = (u % 10) + '0';
    while ((u /= 10)) {
      *--p = (u % 10) + '0';
    }
    break;
  case L_HEX:
    hex = l_hex_digit[(flags & L_UPPER) != 0].p;
    *--p = hex[u & 0x0f];
    while ((u >>= 4)) {
      *--p = hex[u & 0x0f];
    }
    if (!(flags & L_NOOX)) {
      basechar = (flags & L_UPPER) ? 'X' : 'x';
    }
    flags &= (~L_BASE_MASK);
    break;
  case L_OCT:
    *--p = (u & 0x07) + '0';
    while ((u >>= 3)) {
      *--p = (u & 0x07) + '0';
    }
    if (!(flags & L_NOOX)) {
      basechar = (flags & L_UPPER) ? 'O' : 'o';
    }
    flags &= (~L_BASE_MASK);
    break;
  case L_BIN:
    *--p = (u & 0x01) + '0';
    while ((u >>= 1)) {
      *--p = (u & 0x01) + '0';
    }
    if (!(flags & L_NOOX)) {
      basechar = (flags & L_UPPER) ? 'B' : 'b';
    }
    flags &= (~L_BASE_MASK);
    break;
  default:
    break;
  }

  while (precise > (e - p)) {
    *--p = '0';
  }

  if (basechar) {
    *--p = basechar;
    *--p = '0';
  }

  if (flags & L_MINUS) *--p = '-';
  else if (flags & L_PLUS) *--p = '+';
  else if (flags & L_BLANK) *--p = ' ';

  if (width > e - p) {
    if (fill == 0) {
      fill = ' ';
    }
    if (flags & L_LEFT) {
      l_byte* pa = a;
      l_byte* end = a + width;
      while (p < e) {
        *pa++ = *p++;
      }
      while (pa < end) {
        *pa++ = fill;
      }
    } else {
      while (width > e - p) {
        *--p = fill;
      }
    }
  }

  return l_ostream_write_strn(os, l_strn_p(p, e));
}

L_EXTERN l_int
l_ostream_format_d(l_ostream* os, l_long d, l_umedit flags)
{
  l_ulong n = 0;
  if (d < 0) {
    n = -d;
    flags |= L_MINUS;
  } else {
    n = d;
    flags &= (~L_MINUS);
  }
  return l_ostream_format_u(os, n, flags);
}

static l_byte*
l_format_ulong_dec(l_ulong n, l_byte* p)
{
  l_byte a[80];
  l_byte* s = a;

  *s++ = (n % 10) + '0';

  while ((n /= 10)) {
    *s++ = (n % 10) + '0';
  }

  while (s-- > a) {
    *p++ = *s;
  }

  return p;
}

static l_byte*
l_format_fraction_dec(double f, l_byte* p, int precise)
{
  l_ulong ipart = 0;

  if (f < DBL_EPSILON) {
    *p++ = '0';
    return p;
  }

  if (precise == 0) {
    precise = 80;
  }

  while (f >= DBL_EPSILON && precise-- > 0) {
    ipart = (l_ulong)(f * 10);
    *p++ = (l_byte)(ipart + '0');
    f = f * 10 - ipart;
  }

  return p;
}

L_EXTERN l_int
l_ostream_format_f(l_ostream* os, double f, l_umedit flags)
{
  l_value v = lf(f);
  l_byte a[159];
  l_byte sign = 0;
  l_byte* p = a;
  l_byte* dot = 0;
  l_ulong fraction = 0;
  l_ulong mantissa = 0;
  int exponent = 0;
  int negative = 0;
  l_umedit precise = L_GETP(flags);

  /**
   * Floating Point Components
   * |                  | Sign   | Exponent   | Fraction   | Bias
   * |----              |-----   | -----      |  ----      | ----
   * | Single Precision | 1 [31] |  8 [30-23] | 23 [22-00] | 127
   * | Double Precision | 1 [63] | 11 [62-52] | 52 [51-00] | 1023
   * ------------------------------------------------------------
   * Sign - 0 positive, 1 negative
   * Exponent - represent both positive and negative exponents
   *          - the ectual exponent = Exponent - (127 or 1023)
   *          - exponents of -127/-1023 (all 0s) and 128/1024 (255/2047, all 1s) are reserved for special numbers
   * Mantissa - stored in normalized form, this basically puts the radix point after the first non-zero digit
   *          - the mantissa has effectively 24/53 bits of resolution, by way of 23/52 fraction bits: 1.Fraction
   */

  negative = (v.u & 0x8000000000000000) != 0;
  exponent = (v.u & 0x7ff0000000000000) >> 52;
  fraction = (v.u & 0x000fffffffffffff);

  if (negative) sign = '-';
  else if (flags & L_PLUS) sign = '+';
  else if (flags & L_BLANK) sign = ' ';

  if (exponent == 0 && fraction == 0) {
    if (sign) *p++ = sign;
    *p++ = '0'; *p++ = '.'; dot = p; *p++ = '0';
  } else if (exponent == 0x00000000000007ff) {
    if (fraction == 0) {
      if (sign) *p++ = sign;
      *p++ = 'I'; *p++ = 'N'; *p++ = 'F'; *p++ = 'I'; *p++ = 'N'; *p++ = 'I'; *p++ = 'T'; *p++ = 'Y';
    } else {
      if (flags & L_BLANK) *p++ = ' ';
      *p++ = 'N'; *p++ = 'A'; *p++ = 'N';
    }
  } else {
    if (sign) *p++ = sign;
    if (negative) v.u &= 0x7fffffffffffffff;

    exponent = exponent - 1023;
    mantissa = 0x0010000000000000 | fraction;
    /* intmasks = 0xfff0000000000000; */
    /*                         1.fraction
        [ , , , | , , , | , , ,1|f,r,a,c,t,i,o,n,n,n,...]
        <----------- 12 --------|-------- 52 ----------->    */
    if (exponent < 0) {
      /* only have fraction part */
      #if 0
      if (exponent < -8) {
        intmasks = 0xf000000000000000;
        exponent += 8; /* 0000.00000001fraction * 2^exponent */
        mantissa >>= (-exponent); /* lose lower digits */
      } else {
        intmasks <<= (-exponent);
      }
      *p++ = '0'; dot = p; *p++ = '.';
      l_format_fraction_dec(mantissa, intmasks, p);
      #endif
      *p++ = '0'; *p++ = '.'; dot = p;
      p = l_format_fraction_dec(v.f, p, precise);
    } else {
      if (exponent >= 52) {
        /* only have integer part */
        if (exponent <= 63) { /* 52 + 11 */
          mantissa <<= (exponent - 52);
          p = l_format_ulong_dec(mantissa, p);
          *p++ = '.'; dot = p; *p++ = '0';
        } else {
          exponent -= 63;
          mantissa <<= 11;
          p = l_format_ulong_dec(mantissa, p);
          *p++ = '*'; *p++ = '2'; *p++ = '^';
          p = l_format_ulong_dec(exponent, p);
        }
      } else {
        /* have integer part and fraction part */
        #if 0
        intmasks >>= exponent;
        l_format_ulong_dec((mantissa & intmasks) >> (52 - exponent), p);
        *p++ = '.';
        l_format_fraction_dec(mantissa & (~intmasks), intmasks, p);
        #endif
        l_ulong ipart = (l_ulong)v.f;
        p = l_format_ulong_dec(ipart, p);
        *p++ = '.'; dot = p;
        p = l_format_fraction_dec(v.f - ipart, p, precise);
      }
    }
  }

  if (dot && precise) {
    while (p - dot < precise) {
      *p++ = '0';
    }
  }

  return l_ostream_format_fill(os, a, p, flags);
}

L_EXTERN l_int
l_ostream_format_s(l_ostream* os, const void* s, l_umedit flags)
{
  return l_ostream_format_strn(os, l_strn_c(s), flags);
}

L_EXTERN l_int
l_ostream_format_bool(l_ostream* os, int n, l_umedit flags)
{
  if (n) {
    if (flags & L_UPPER) {
      return l_ostream_format_strn(os, l_literal_strn("TRUE"), flags);
    } else {
      return l_ostream_format_strn(os, l_literal_strn("true"), flags);
    }
  } else {
    if (flags & L_UPPER) {
      return l_ostream_format_strn(os, l_literal_strn("FALSE"), flags);
    } else {
      return l_ostream_format_strn(os, l_literal_strn("false"), flags);
    }
  }
}

static const l_byte*
l_ostream_format_a_value(l_ostream* os, const l_byte* start, const l_byte* end, l_value a)
{
  /** format flags **
  s - const void*
  f - double
  d - l_long
  u - l_ulong
  strn - l_strn*
  bool - print true or false
  p - print as pointer value
  b - bin
  o - oct
  x - hex
  base - (b)in (o)ct (h)ex
  sign - ( )blank (+)plus (z)dont print 0b 0o 0x prefix
  justify - (l)eft, default is right
  width - 1 ~ 2 digit
  precision - (.) and 1 ~ 2 digit
  fill - fill to reach the width length
  ********************************************************************/
  l_umedit flags = 0; /* start point to '%' and next char is not '%' */
  const l_byte* cur = start;

  while (++cur < end) {
    switch (*cur) {
    case ' ':
      flags |= L_BLANK;
      continue;
    case '+':
      flags |= L_PLUS;
      continue;
    case '.':
      flags |= L_PRECISE;
      continue;
    case 'l': case 'L':
      flags |= L_LEFT;
      continue;
    case 'z': case 'Z':
      flags |= L_NOOX;
      continue;
    case '0': case '~': case '-': case '=': case '#':
      flags |= L_SETF(*cur); /* fill char */
      continue;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
      l_umedit width = *cur - '0';
      l_byte ch = 0;
      if (cur + 1 < end && (ch = *(cur + 1)) >= '0' && ch <= '9') {
        /* cur + 1 is the 2nd digit */
        width = width * 10 + ch - '0';
        ++cur;
        /* skip next extra digits */
        while (cur + 1 < end && (ch = *(cur + 1)) >= '0' && ch <= '9') {
          ++cur;
        }
      }
      /* current char is digit, and next is not digit or end */
      if (flags & L_PRECISE) {
        flags |= L_SETP(width);
        flags &= (~L_PRECISE);
      } else {
        flags |= L_SETW(width);
      }}
      continue;
    case 'S':
      flags |= L_UPPER;
      /* fallthrough */
    case 's':
      if (end - cur >= 4) {
        if (*(cur+1) == 't' && *(cur+2) == 'r' && *(cur+3) == 'n') {
          l_ostream_format_strn(os, *((l_strn*)a.p), flags);
          return cur + 4;
        } else if (*(cur+1) == 'v' && *(cur+2) == 'i' && *(cur+3) == 'd') {
          l_ostream_format_u(os, a.u, flags | L_SETP(16));
          return cur + 4;
        }
      }
      l_ostream_format_s(os, a.p, flags);
      return cur + 1;
    case 'f': case 'F':
      l_ostream_format_f(os, a.f, flags);
      return cur + 1;
    case 'u': case 'U':
      l_ostream_format_u(os, a.u, flags);
      return cur + 1;
    case 'd': case 'D':
      l_ostream_format_d(os, a.d, flags);
      return cur + 1;
    case 'C':
      flags |= L_UPPER;
      /* fallthrough */
    case 'c':
      l_ostream_format_c(os, (int)a.u, flags);
      return cur + 1;
    case 'B':
      flags |= L_UPPER;
      /* fallthrough */
    case 'b':
      if (end - cur >= 4 && *(cur+1) == 'o' && *(cur+2) == 'o' && *(cur+3) == 'l') {
        l_ostream_format_bool(os, (int)a.u, flags);
        return cur + 4;
      }
      flags |= L_BIN;
      l_ostream_format_u(os, a.u, flags);
      return cur + 1;
    case 'O':
      flags |= L_UPPER;
      /* fallthrough */
    case 'o':
      flags |= L_OCT;
      l_ostream_format_u(os, a.u, flags);
      return cur + 1;
    case 'X': case 'P':
      flags |= L_UPPER;
      /* fallthrough */
    case 'x': case 'p':
      flags |= L_HEX;
      l_ostream_format_u(os, a.u, flags);
      return cur + 1;
    default:
      break;
    }

    /* wrong format if goes here */
    break;
  }

  return 0;
}

typedef union {
  const l_value* a;
  va_list vl;
} l_format_nval;

static int
l_impl_ostream_format_n(l_ostream* os, const void* fmt, l_int n, l_format_nval* val, int valist)
{
  int nfmts = 0;
  const l_byte* cur = 0;
  const l_byte* end = 0;
  const l_byte* beg = 0;
  l_value cur_val;
  l_strn fmt_str;

  if (fmt == 0) {
    return 0;
  }

  fmt_str = l_strn_c(fmt);

  if (n <= 0) {
    l_ostream_write(os, fmt_str.p, fmt_str.n);
    return 0;
  }

  beg = fmt_str.p;
  end = beg + fmt_str.n;
  cur = beg;

  while (cur < end) {
    if (*cur != '%') {
      ++cur;
      continue;
    }

    if (cur + 1 == end) {
      /* cur is '%' and next is end */
      l_ostream_write_strn(os, l_strn_p(beg, cur + 1)); /* '%' is printed */
      return nfmts;
    } else if (cur + 1 < end && *(cur + 1) == '%') {
      /* cur is '%' and next is '%' */
      l_ostream_write_strn(os, l_strn_p(beg, cur + 1));
      cur = cur + 2; /* cur now pointer to 1 position after "%%" */
      beg = cur;
    } else {
      /* cur is '%' and next is not '%' */
      l_ostream_write_strn(os, l_strn_p(beg, cur));

      if (valist) {
        cur_val = va_arg(val->vl, l_value);
      } else {
        cur_val = val->a[nfmts];
      }

      cur = l_ostream_format_a_value(os, cur, end, cur_val);
      if (cur == 0) { /* current format is invalid */
        l_ostream_write_strn(os, l_strn_p(cur, end));
        return nfmts;
      }

      beg = cur;
      if (++nfmts == n) {
        break;
      }
    }
  }

  if (beg < end) {
    l_ostream_write_strn(os, l_strn_p(beg, end));
  }

  return nfmts;
}

L_EXTERN int
l_ostream_format_n(l_ostream* os, const void* fmt, l_int n, const l_value* a)
{
  l_format_nval val;
  val.a = a;
  return l_impl_ostream_format_n(os, fmt, n, &val, false);
}

L_EXTERN int
l_impl_ostream_format_v(l_ostream* os, const void* fmt, l_int n, va_list vl)
{
  /** variable arguments **
  The type va_list is an object type suitable for holding information
  needed by the macros va_start, va_arg, va_end, and va_copy. If access
  to the varying arguments is desired, the called function shall decalre
  an object such as ap having type va_list. The object ap may be passed
  as an argument to another function; if that function invokes the va_arg
  macro with parameter ap, the value of ap in the calling function is
  indeterminate and shall be passed to the va_end macro prior to any
  further reference to ap. 215) It is permitted to create a pointer to a
  va_list and pass that pointer to another function, in which case the
  original function may make further use of the original list after
  the other function returns.
  Each invocation of the va_arg macro modifies ap so that the values of
  successive arguments are returned in turn.
  Cannot pass a pointer of va_list and then va_arg(*vl, l_value) in linux.
  It will receive a signal of SIGSEGV, Segmentation fault. **/

  int cnt = 0;
  l_format_nval val;
  va_copy(val.vl, vl);
  cnt = l_impl_ostream_format_n(os, fmt, n, &val, true);
  va_end(val.vl);
  return cnt;
}

L_EXTERN int
l_impl_ostream_format(l_ostream* os, const void* fmt, l_int n, ...)
{
  int nfmts = 0;
  va_list vl;
  va_start(vl, n);
  nfmts = l_impl_ostream_format_v(os, fmt, n, vl);
  va_end(vl);
  return nfmts;
}

/** fixed length string buffer **/

typedef struct l_strbuf {
  l_int total;
  l_int n;
  l_byte s[1];
} l_strbuf;

static l_int
l_impl_strbuf_write(void* out, const void* p, l_int len)
{
  l_strbuf* b = (l_strbuf*)out;
  if (p == 0 || len <= 0) {
    return 0;
  }
  if (b->n + len < b->total) {
    l_copy_n(b->s + b->n, p, len);
    b->n += len;
    b->s[b->n] = 0;
    return len;
  } else {
    return 0;
  }
}

static l_strbuf*
l_strbuf_init(void* p, l_int total)
{
  l_strbuf* b = (l_strbuf*)p;
  b->total = total - 1;
  b->n = 0;
  b->s[0] = 0;
  return b;
}

L_EXTERN l_ostream
l_strbuf_ostream(l_strbuf* b)
{
  return l_ostream_from(b, l_impl_strbuf_write, 0);
}

L_EXTERN l_int
l_strbuf_write(l_strbuf* b, l_strn s)
{
  return l_impl_strbuf_write(b, s.p, s.n);
}

L_EXTERN void
l_strbuf_clear(l_strbuf* b)
{
  b->n = 0;
  b->s[0] = 0;
}

L_EXTERN l_int
l_strbuf_reset(l_strbuf* b, l_strn s)
{
  l_strbuf_clear(b);
  return l_strbuf_write(b, s);
}

L_EXTERN const l_byte*
l_strbuf_strc(l_strbuf* b)
{
  return b->s;
}

L_EXTERN l_int
l_strbuf_size(l_strbuf* b)
{
  return b->n;
}

L_EXTERN l_int
l_strbuf_capacity(l_strbuf* b)
{
  return b->total;
}

L_EXTERN l_byte*
l_strbuf_getp(l_strbuf* b)
{
  return b->s;
}

L_EXTERN void
l_strbuf_add_len(l_strbuf* b, l_int n)
{
  b->n += n;
}

L_EXTERN void
l_strbuf_adjust_len(l_strbuf* b)
{
  b->n = strlen((const char*)b->s);
}

L_EXTERN l_strn
l_strbuf_strn(l_strbuf* b)
{
  return l_strn_l(b->s, b->n);
}

L_EXTERN l_bool
l_strbuf_is_empty(l_strbuf* b)
{
  return b->s[0] == 0;
}

L_EXTERN l_bool
l_strbuf_nt_empty(l_strbuf* b)
{
  return b->s[0] != 0;
}

L_EXTERN l_int
l_strbuf_add_path(l_strbuf* b, l_strn path)
{
  L_UNUSED(b);
  L_UNUSED(path);
  return 0;
}

L_EXTERN l_int
l_strbuf_end_path(l_strbuf* b, l_strn filename)
{
  L_UNUSED(b);
  L_UNUSED(filename);
  return 0;
}

L_EXTERN l_int
l_strbuf_end_path_x(l_strbuf* b, l_strn name_parta, l_strn name_partb)
{
  L_UNUSED(b);
  L_UNUSED(name_parta);
  L_UNUSED(name_partb);
  return 0;
}

#if 0
L_EXTERN void
l_filename_init(l_filename* nm)
{
  nm->buff_len = L_MAX_FILENAME - 8;
  nm->name_len = 0;
  nm->s[0] = 0;
}

L_EXTERN l_bool
l_filename_set(l_filename* nm, l_strn s)
{
  l_filename_init(nm);
  return l_filename_append(nm, s);
}

L_EXTERN l_bool
l_filename_append(l_filename* nm, l_strn s)
{
  if (s.n > 0 && nm->name_len + s.n < nm->buff_len) {
    l_copy_n(nm->s + nm->name_len, s.p, s.n);
    nm->name_len += s.n;
    nm->s[nm->name_len] = 0;
    return true;
  } else {
    return false;
  }
}

static l_int
l_filename_write(void* out, const void* p, l_int len)
{
  l_filename* self = (l_filename*)out;
  if (p == 0 || len <= 0) {
    return 0;
  }
  if (self->name_len + len < self->buff_len) {
    l_copy_n(self->s + self->name_len, p, len);
    self->name_len += len;
    self->s[self->name_len] = 0;
    return len;
  } else {
    return 0;
  }
}

L_EXTERN l_ostream
l_filename_ostream(l_filename* out)
{
  l_ostream os;
  os.out = out;
  os.size = 0;
  os.write = l_filename_write;
  return os;
}

L_EXTERN l_bool
l_filename_addname(l_filename* nm, l_strn name, l_strn suffix)
{
  return l_filename_append(nm, name) && l_filename_append(nm, suffix);
}

L_EXTERN l_bool
l_filename_addname_combine(l_filename* nm, l_strn part1, l_strn part2, l_strn sep)
{
  return l_filename_append(nm, part1) && l_filename_append(nm, sep) && l_filename_append(nm, part2);
}

L_EXTERN l_bool
l_filename_addpath(l_filename* nm, l_strn path)
{
  if (path.n > 0 && nm->name_len + path.n < nm->buff_len) {
    const l_byte* pend = path.p + path.n;
    if (nm->name_len > 0) {
      if (nm->s[nm->name_len - 1] == '/') {
        if (*path.p == '/') {
          path.p += 1;
        }
      } else {
        if (*path.p != '/') {
          nm->s[nm->name_len++] = '/';
        }
      }
    }
    while (path.p < pend) {
      nm->s[nm->name_len++] = *path.p++;
    }
    if (nm->s[nm->name_len - 1] != '/') {
      nm->s[nm->name_len++] = '/';
    }
    nm->s[nm->name_len] = 0;
    return true;
  } else {
    return false;
  }
}
#endif

L_EXTERN l_strbuf*
l_sbuf16_init(l_sbuf16* b)
{
  return l_strbuf_init(b, 16);
}

L_EXTERN l_strbuf*
l_sbuf32_init(l_sbuf32* b)
{
  return l_strbuf_init(b, 32);
}

L_EXTERN l_strbuf*
l_sbuf64_init(l_sbuf64* b)
{
  return l_strbuf_init(b, 64);
}

L_EXTERN l_strbuf*
l_sbuf12_init(l_sbuf12* b)
{
  return l_strbuf_init(b, 128);
}

L_EXTERN l_strbuf*
l_sbuf25_init(l_sbuf25* b)
{
  return l_strbuf_init(b, 256);
}

L_EXTERN l_strbuf*
l_sbuf51_init(l_sbuf51* b)
{
  return l_strbuf_init(b, 512);
}

L_EXTERN l_strbuf*
l_sbuf1k_init(l_sbuf1k* b)
{
  return l_strbuf_init(b, 1024);
}

L_EXTERN l_strbuf*
l_sbuf2k_init(l_sbuf2k* b)
{
  return l_strbuf_init(b, 1024*2);
}

L_EXTERN l_strbuf*
l_sbuf3k_init(l_sbuf3k* b)
{
  return l_strbuf_init(b, 1024*3);
}

L_EXTERN l_strbuf*
l_sbuf4k_init(l_sbuf4k* b)
{
  return l_strbuf_init(b, 1024*4);
}

L_EXTERN l_strbuf*
l_sbuf5k_init(l_sbuf5k* b)
{
  return l_strbuf_init(b, 1024*5);
}

L_EXTERN l_strbuf*
l_sbuf6k_init(l_sbuf6k* b)
{
  return l_strbuf_init(b, 1024*6);
}

L_EXTERN l_strbuf*
l_sbuf7k_init(l_sbuf7k* b)
{
  return l_strbuf_init(b, 1024*7);
}

L_EXTERN l_strbuf*
l_sbuf8k_init(l_sbuf8k* b)
{
  return l_strbuf_init(b, 1024*8);
}

L_EXTERN l_strbuf*
l_sbuf16_init_from(l_sbuf16* b, l_strn s)
{
  l_strbuf* p = l_sbuf16_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf32_init_from(l_sbuf32* b, l_strn s)
{
  l_strbuf* p = l_sbuf32_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf64_init_from(l_sbuf64* b, l_strn s)
{
  l_strbuf* p = l_sbuf64_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf12_init_from(l_sbuf12* b, l_strn s)
{
  l_strbuf* p = l_sbuf12_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf25_init_from(l_sbuf25* b, l_strn s)
{
  l_strbuf* p = l_sbuf25_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf51_init_from(l_sbuf51* b, l_strn s)
{
  l_strbuf* p = l_sbuf51_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf1k_init_from(l_sbuf1k* b, l_strn s)
{
  l_strbuf* p = l_sbuf1k_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf2k_init_from(l_sbuf2k* b, l_strn s)
{
  l_strbuf* p = l_sbuf2k_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf3k_init_from(l_sbuf3k* b, l_strn s)
{
  l_strbuf* p = l_sbuf3k_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf4k_init_from(l_sbuf4k* b, l_strn s)
{
  l_strbuf* p = l_sbuf4k_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf5k_init_from(l_sbuf5k* b, l_strn s)
{
  l_strbuf* p = l_sbuf5k_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf6k_init_from(l_sbuf6k* b, l_strn s)
{
  l_strbuf* p = l_sbuf6k_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf7k_init_from(l_sbuf7k* b, l_strn s)
{
  l_strbuf* p = l_sbuf7k_init(b);
  l_strbuf_write(p, s);
  return p;
}

L_EXTERN l_strbuf*
l_sbuf8k_init_from(l_sbuf8k* b, l_strn s)
{
  l_strbuf* p = l_sbuf8k_init(b);
  l_strbuf_write(p, s);
  return p;
}

/** variable length string **/

#define L_STRING_FIXLEN_CAP (2 * sizeof(void*) - 1)

L_EXTERN l_int
l_string_capacity(l_string* s)
{
  if (s->implsz <= 0) {
    return L_STRING_FIXLEN_CAP;
  } else {
    return s->impltt;
  }
}

L_EXTERN l_string
l_string_init(l_allocfunc alloc, l_int size)
{
  l_string s = {l_rawalloc_func, 0, 0, 0};
  if (alloc) {
    s.alloc = alloc;
  }
  if (size > (l_int)L_STRING_FIXLEN_CAP) {
    l_byte* newp = 0;
    l_int capacity = (L_STRING_FIXLEN_CAP + 1) * 2 - 1;
    if (size > capacity) {
      capacity = size;
    }
    newp = s.alloc(0, 0, 0, capacity + 1);
    newp[0] = 0;
    s.implsz = 0;
    s.impltt = capacity;
    s.lnstr = newp;
  }
  return s;
}

static l_int
l_impl_string_write(void* out, const void* p, l_int len)
{
  l_string* s = (l_string*)out;
  if (p == 0 || len <= 0) {
    return 0;
  } else {
    l_int size = s->implsz;
    l_byte* oldp = l_string_strc(s);
    if (size <= 0) {
      size = -size;
      if (size + len <= (l_int)L_STRING_FIXLEN_CAP) {
        l_copy_n(oldp + size, p, len);
        size += len;
        oldp[size] = 0;
        s->implsz = -size;
      } else {
        l_byte* newp = 0;
        l_int capacity = (L_STRING_FIXLEN_CAP + 1) * 2 - 1;
        if (size + len > capacity) {
          capacity = size + len;
        }
        newp = s->alloc(0, 0, 0, capacity + 1);
        l_copy_n(newp, oldp, size);
        l_copy_n(newp + size, p, len);
        size += len;
        newp[size] = 0;
        s->implsz = size;
        s->impltt = capacity;
        s->lnstr = newp;
      }
    } else {
      if (size + len <= s->impltt) {
        l_copy_n(oldp + size, p, len);
        size += len;
        oldp[size] = 0;
        s->implsz = size;
      } else {
        l_byte* newp = 0;
        l_int capacity = (s->impltt + 1) * 2 - 1;
        if (size + len > capacity) {
          capacity = size + len;
        }
        if (capacity <= s->impltt) {
          l_loge_1(LNUL, "string too long %d", ld(s->impltt));
          return 0;
        }
        newp = s->alloc(0, oldp, 0, capacity + 1);
        l_copy_n(newp + size, p, len);
        size += len;
        newp[size] = 0;
        s->implsz = size;
        s->impltt = capacity;
        s->lnstr = newp;
      }
    }
    return len;
  }
}

L_EXTERN l_ostream
l_string_ostream(l_string* s)
{
  return l_ostream_from(s, l_impl_string_write, 0);
}

L_EXTERN l_int
l_string_write(l_string* s, l_strn from)
{
  return l_impl_string_write(s, from.p, from.n);
}

L_EXTERN l_string
l_string_init_from(l_allocfunc alloc, l_int size, l_strn from)
{
  l_string s = l_string_init(alloc, size);
  l_string_write(&s, from);
  return s;
}

L_EXTERN void
l_string_clear(l_string* s)
{
  if (s->implsz <= 0) {
    *s = l_string_init(s->alloc, 0);
  } else {
    if (s->lnstr) {
      s->alloc(0, s->lnstr, 0, 0);
      s->lnstr = 0;
    }
    *s = l_string_init(s->alloc, 0);
  }
}

L_EXTERN l_int
l_string_reset(l_string* s, l_strn from)
{
  l_string_clear(s);
  return l_string_write(s, from);
}

/** standard file stream **/

static l_file
l_impl_file_open(const void* name, const char* mode)
{
  l_file s = {0};
  if (name && mode) {
    s.file = fopen((const char*)name, mode);
    if (s.file == 0) {
      l_loge_2(LNUL, "fopen %s %s", ls(name), lserror(errno));
    }
  } else {
    l_loge_s(LNUL, "EINVAL");
  }
  return s;
}

static l_file
l_impl_file_open_nobuf(const void* name, const char* mode)
{
  l_file s = l_impl_file_open(name, mode);
  if (s.file) {
    setbuf((FILE*)s.file, 0);
  }
  return s;
}

static void
l_impl_file_reopen(FILE* file, const void* name, const char* mode)
{
  if (name && mode) {
    if (freopen((const char*)name, mode, file) == 0) {
      l_loge_2(LNUL, "freopen %s %s", ls(name), lserror(errno));
    }
  } else {
    l_loge_s(LNUL, "EINVAL");
  }
}

L_EXTERN l_file
l_file_open_read(const void* name)
{
  return l_impl_file_open(name, "rb");
}

L_EXTERN l_file
l_file_open_read_nobuf(const void* name)
{
  return l_impl_file_open_nobuf(name, "rb");
}

L_EXTERN l_file
l_file_open_write(const void* name)
{
  return l_impl_file_open(name, "wb");
}

L_EXTERN l_file
l_file_open_write_nobuf(const void* name)
{
  return l_impl_file_open_nobuf(name, "wb");
}

L_EXTERN l_file
l_file_open_append(const void* name)
{
  return l_impl_file_open(name, "ab");
}

L_EXTERN l_file
l_file_open_append_nobuf(const void* name)
{
  return l_impl_file_open_nobuf(name, "ab");
}

L_EXTERN l_file
l_file_open_read_write(const void* name)
{
  return l_impl_file_open(name, "rb+");
}

L_EXTERN void
l_file_close(l_file* s)
{
  if (s->file == 0) {
    return;
  }
  if (fclose((FILE*)s->file) != 0) {
    l_loge_1(LNUL, "fclose %s", lserror(errno));
  }
  s->file = 0;
}

L_EXTERN void
l_file_clearerr(l_file* s)
{
  clearerr((FILE*)s->file);
}

L_EXTERN l_bool
l_file_flush(l_file* s)
{
  if (fflush((FILE*)s->file) == 0) {
    return true;
  } else {
    l_loge_1(LNUL, "fflush %s", lserror(errno));
    return false;
  }
}

L_EXTERN l_bool
l_file_rewind(l_file* s)
{
  if (fseek((FILE*)s->file, 0, SEEK_SET) == 0) {
    return true;
  } else {
    l_loge_1(LNUL, "fseek SET %s", lserror(errno));
    return false;
  }
}

L_EXTERN l_bool
l_file_seekto(l_file* s, l_int pos)
{
  if (pos <= 0) {
    return false;
  }
  if (fseek((FILE*)s->file, pos, SEEK_SET) == 0) {
    return true;
  } else {
    l_loge_2(LNUL, "fseek SET %d %s", ld(pos), lserror(errno));
    return false;
  }
}

L_EXTERN l_bool
l_file_forword(l_file* s, l_int offset)
{
  if (offset <= 0) {
    return false;
  }
  if (fseek((FILE*)s->file, offset, SEEK_CUR) == 0) {
    return true;
  } else {
    l_loge_2(LNUL, "fseek CUR %d %s", ld(offset), lserror(errno));
    return false;
  }
}

L_EXTERN l_bool
l_file_backward(l_file* s, l_int offset)
{
  if (offset <= 0) {
    return false;
  }
  if (fseek((FILE*)s->file, -offset, SEEK_CUR) == 0) {
    return true;
  } else {
    l_loge_2(LNUL, "fseek CUR %d %s", ld(offset), lserror(errno));
    return false;
  }
}

L_EXTERN l_int
l_file_read(l_file* s, void* out, l_int size)
{
  l_int n = 0;
  if (out == 0 || size <= 0) {
    return 0;
  }
  n = (l_int)fread(out, 1, (size_t)size, (FILE*)s->file);
  if (n == size) {
    return n;
  }
  if (!feof((FILE*)s->file)) {
    l_loge_1(LNUL, "fread %s", lserror(errno));
  }
  return n < 0 ? 0 : n;
}

static l_int
l_impl_file_write(void* out, const void* p, l_int len)
{
  l_int n = 0;

  n = (l_int)fwrite(p, 1, (size_t)len, (FILE*)out);
  if (n == len) {
    return n;
  }

  l_loge_1(LNUL, "fwrite %s", lserror(errno));
  return (n < 0 ? 0 : n);
}

static void
l_impl_file_flush(void* out)
{
  fflush((FILE*)out);
}

L_EXTERN l_int
l_file_write(l_file* s, const void* p, l_int len)
{
  l_int n = 0;

  if (p == 0 || len <= 0) {
    return 0;
  }

  n = (l_int)fwrite(p, 1, (size_t)len, (FILE*)s->file);
  if (n == len) {
    return n;
  }

  l_loge_1(LNUL, "fwrite %s", lserror(errno));
  return (n < 0 ? 0 : n);
}

L_EXTERN l_ostream
l_stdout_ostream()
{
  return l_ostream_from(stdout, l_impl_file_write, l_impl_file_flush);
}

L_EXTERN l_ostream
l_stderr_ostream()
{
  return l_ostream_from(stderr, l_impl_file_write, l_impl_file_flush);
}

L_EXTERN l_int
l_file_write_strn(l_file* out, l_strn s)
{
  return l_file_write(out, s.p, s.n);
}

L_EXTERN l_int
l_file_put(l_file* s, l_byte ch)
{
  if (fwrite(&ch, 1, 1, (FILE*)s->file) != 1) {
    l_loge_1(LNUL, "fwrite %s", lserror(errno));
    return 0;
  }
  return 1;
}

L_EXTERN l_int
l_file_get(l_file* s, l_byte* ch)
{
  if (fread(ch, 1, 1, (FILE*)s->file) != 1) {
    if (!feof((FILE*)s->file)) {
      l_loge_1(LNUL, "fread %s", lserror(errno));
    }
    return 0;
  }
  return 1;
}

L_EXTERN l_bool
l_file_remove(const void* name)
{
  if (name) {
    if (remove((const char*)name) == 0) {
      return true;
    } else {
      l_loge_1(LNUL, "remove %s", lserror(errno));
      return false;
    }
  } else {
    l_loge_s(LNUL, "EINVAL");
    return false;
  }
}

L_EXTERN l_bool
l_file_rename(const void* from, const void* to)
{
  /* int rename(const char* oldname, const char* newname);
   * Changes the name of the file or directory specified by oldname
   * to newname. This is an operation performed directly on a file;
   * No streams are involved in the operation. If oldname and newname
   * specify different paths and this is supported by the system, the
   * file is moved to the new location. If newname names an existing
   * file, the function may either fail or override the existing file,
   * depending on the specific system and library implementation.
   */
  if (from && to) {
    if (rename((const char*)from, (const char*)to) == 0) {
      return true;
    } else {
      l_loge_1(LNUL, "rename %s", lserror(errno));
      return false;
    }
  } else {
    l_loge_s(LNUL, "EINVAL");
    return false;
  }
}

L_EXTERN void
l_file_redirect_stdout(const void* name)
{
  l_impl_file_reopen(stdout, name, "wb");
}

L_EXTERN void
l_file_redirect_stderr(const void* name)
{
  l_impl_file_reopen(stderr, name, "wb");
}

L_EXTERN void
l_file_redirect_stdin(const void* name)
{
  l_impl_file_reopen(stdin, name, "rb");
}

