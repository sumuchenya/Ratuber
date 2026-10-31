# ============================================================
# Ratuber - CosyVoice Async TTS Server
#
# 文件位置：
#
#   D:\Ratuber\Ratuber\AI\Server\tts_server.py
#
# 功能：
#   1. CosyVoice 常驻内存，只加载一次
#   2. POST /tts 不等待语音生成，立即返回 task_id
#   3. 后台 TTS Worker 按 FIFO 顺序处理任务
#   4. 自动按标点拆分小句，每个小句独立生成 WAV 文件
#   5. 文件名包含小句序号，例如 _part_02_of_04.wav
#   6. GET /tts/{task_id} 查询整句及各小句状态
#   7. GET /audio/{task_id} 获取第一个小句（兼容旧接口）
#   8. GET /audio/{task_id}/{segment_index} 获取指定小句
#
# 工作方式：
#
#   Ratuber AI
#       │
#       │ 提交完整文本
#       ▼
#   POST /tts
#       │
#       ├── 按标点拆分小句，立即返回 task_id 和小句清单
#       ▼
#   TTS Queue（按整段请求 FIFO）
#       │
#       ▼
#   CosyVoice Worker（逐个小句独立生成）
#       │
#       ▼
#   tempvoice/{task_id}_part_01_of_03.wav
#   tempvoice/{task_id}_part_02_of_03.wav
#   tempvoice/{task_id}_part_03_of_03.wav
#
# ============================================================

import os
import sys
import uuid
import re
import wave
import queue
import time
import threading
import logging
from logging.handlers import RotatingFileHandler
from datetime import datetime


# ============================================================
# 0. 基础路径
# ============================================================

# ------------------------------------------------------------
# 当前服务器文件所在目录
#
# D:\Ratuber\Ratuber\AI\Server
# ------------------------------------------------------------

SERVER_DIR = os.path.dirname(
    os.path.abspath(__file__)
)


# ------------------------------------------------------------
# Ratuber 项目根目录
#
# SERVER_DIR
#     ↓ .. = AI
#     ↓ .. = Ratuber
#
# D:\Ratuber\Ratuber
# ------------------------------------------------------------

PROJECT_DIR = os.path.abspath(
    os.path.join(
        SERVER_DIR,
        "..",
        ".."
    )
)


# ------------------------------------------------------------
# CosyVoice 资源目录
#
# D:\Ratuber\Ratuber\assets\CosyVoice
# ------------------------------------------------------------

COSYVOICE_DIR = os.path.join(
    PROJECT_DIR,
    "assets",
    "CosyVoice"
)


# ============================================================
# CosyVoice 资源路径
# ============================================================

# CosyVoice 模型

MODEL_DIR = os.path.join(
    COSYVOICE_DIR,
    "pretrained_models",
    "CosyVoice2-0.5B"
)


# 参考音频

REF_WAV = os.path.join(
    COSYVOICE_DIR,
    "voice.wav"
)


# 参考音频对应的文字

PROMPT_TEXT_FILE = os.path.join(
    COSYVOICE_DIR,
    "test.txt"
)


# wetext 本地规则

WETEXT_DIR = os.path.join(
    COSYVOICE_DIR,
    "wetext_rules"
)


# 生成的 WAV 文件（临时语音，和 CosyVoice 环境分开）
#
# D:\Ratuber\Ratuber\tempvoice

OUTPUT_DIR = os.path.join(
    PROJECT_DIR,
    "tempvoice"
)

os.makedirs(
    OUTPUT_DIR,
    exist_ok=True
)


# ============================================================
# 日志
# ============================================================

# 日志放在 CosyVoice 资源目录。
#
# 这样和原来的位置保持一致：
#
# D:\Ratuber\Ratuber\assets\CosyVoice\tts_server.log
#
# 以后 AI/Server 里面不会出现一堆模型运行产生的大日志。

LOG_FILE = os.path.join(
    COSYVOICE_DIR,
    "tts_server.log"
)


logger = logging.getLogger(
    "RatuberTTS"
)

logger.setLevel(
    logging.INFO
)


formatter = logging.Formatter(
    "[%(asctime)s] [%(levelname)s] %(message)s"
)


console_handler = logging.StreamHandler()

console_handler.setFormatter(
    formatter
)


file_handler = RotatingFileHandler(
    LOG_FILE,
    maxBytes=5 * 1024 * 1024,
    backupCount=3,
    encoding="utf-8"
)

file_handler.setFormatter(
    formatter
)


# 防止以后这个模块被重复 import 时
# 添加多个完全相同的 Handler

if not logger.handlers:

    logger.addHandler(
        console_handler
    )

    logger.addHandler(
        file_handler
    )


# ============================================================
# 第三方 Python 路径
# ============================================================

# ------------------------------------------------------------
# CosyVoice 本体
#
# 现在不在 Server 目录，
# 而是在：
#
# D:\Ratuber\Ratuber\assets\CosyVoice
# ------------------------------------------------------------

if COSYVOICE_DIR not in sys.path:

    sys.path.insert(
        0,
        COSYVOICE_DIR
    )


# ------------------------------------------------------------
# Matcha-TTS
# ------------------------------------------------------------

MATCHA_DIR = os.path.join(
    COSYVOICE_DIR,
    "third_party",
    "Matcha-TTS"
)


if os.path.isdir(
    MATCHA_DIR
):

    if MATCHA_DIR not in sys.path:

        sys.path.insert(
            0,
            MATCHA_DIR
        )


# ============================================================
# wetext
#
# 防止 wetext 运行时尝试联网下载规则。
# 使用本地 wetext_rules。
# ============================================================

try:

    import wetext.wetext as _ww

    if os.path.isdir(
        WETEXT_DIR
    ):

        _ww.snapshot_download = (
            lambda *args, **kwargs:
            WETEXT_DIR
        )

    logger.info(
        "wetext initialized."
    )

except Exception as e:

    logger.warning(
        "wetext initialization warning: %s",
        e
    )


# ============================================================
# 导入 FastAPI / CosyVoice
# ============================================================

from fastapi import (
    FastAPI,
    HTTPException
)

from fastapi.responses import FileResponse

from pydantic import BaseModel

import uvicorn

import numpy as np

from cosyvoice.cli.cosyvoice import CosyVoice2


# ============================================================
# 配置
# ============================================================

HOST = "127.0.0.1"

PORT = 8000


# 单个句子最大字符数

MAX_CHARS = 200

# 拆句规则：
#
#   文字不超过 WHOLE_SENTENCE_MAX_CHARS 个字：整句一次生成，不拆。
#   更长的：只在 。！？；… 和换行处拆（不在逗号处拆），
#   每个小句至少 MIN_SEGMENT_CHARS 个字，太短的并进下一句。
#
# 想永远整句生成：把 WHOLE_SENTENCE_MAX_CHARS 改成 200（等于 MAX_CHARS）。

WHOLE_SENTENCE_MAX_CHARS = 25

MIN_SEGMENT_CHARS = 12
# Speaker ID

SPK_ID = "myvoice"


# 是否使用 FP16

USE_FP16 = True


# 队列最大任务数量

MAX_QUEUE_SIZE = 32


# ============================================================
# FastAPI
# ============================================================

app = FastAPI(
    title="Ratuber CosyVoice TTS Server",
    version="1.1.0"
)


# ============================================================
# 请求结构
# ============================================================

class TTSRequest(BaseModel):

    text: str


# ============================================================
# 全局状态
# ============================================================

cv = None

SAMPLE_RATE = 22050


tts_queue = queue.Queue(
    maxsize=MAX_QUEUE_SIZE
)


worker_thread = None

worker_running = False


# 保存所有任务信息

jobs = {}


# jobs 的锁

jobs_lock = threading.Lock()


# 防止 CosyVoice 同时运行多个 inference

inference_lock = threading.Lock()


# ============================================================
# 工具函数
# ============================================================

def now_string():

    """
    返回当前时间字符串。
    """

    return datetime.now().isoformat(
        timespec="seconds"
    )


def read_prompt_text():

    """
    读取参考音频对应的文字。

    优先 UTF-8，
    如果失败再尝试 UTF-8-SIG / GBK。
    """

    encodings = [
        "utf-8",
        "utf-8-sig",
        "gbk"
    ]

    last_error = None

    for encoding in encodings:

        try:

            with open(
                PROMPT_TEXT_FILE,
                "r",
                encoding=encoding
            ) as f:

                text = f.read().strip()

            if text:

                return text

        except Exception as e:

            last_error = e

    raise RuntimeError(
        f"无法读取参考文本: "
        f"{PROMPT_TEXT_FILE}, "
        f"最后错误: {last_error}"
    )


def tensor_to_pcm(audio_tensor):

    """
    CosyVoice 输出 tensor
    转换成 16-bit PCM bytes。
    """

    audio = (
        audio_tensor
        .detach()
        .cpu()
        .numpy()
    )

    # 如果是二维，例如 [1, samples]

    if audio.ndim > 1:

        audio = audio.reshape(
            -1
        )

    # 防止数值超出范围

    audio = np.clip(
        audio,
        -1.0,
        1.0
    )

    pcm = (
        audio * 32767.0
    ).astype(
        np.int16
    )

    return pcm.tobytes()


def save_wav(
    filepath,
    pcm_data,
    sample_rate
):

    """
    将 PCM 数据保存成 WAV。

    使用临时文件 + os.replace，
    防止客户端读取到一半生成的 WAV。
    """

    temp_path = (
        filepath + ".tmp"
    )

    with wave.open(
        temp_path,
        "wb"
    ) as wf:

        wf.setnchannels(1)

        wf.setsampwidth(2)

        wf.setframerate(
            sample_rate
        )

        wf.writeframes(
            pcm_data
        )

    # 原子替换

    os.replace(
        temp_path,
        filepath
    )


def split_text_into_segments(text):

    """
    拆小句。

    1. 文字不超过 WHOLE_SENTENCE_MAX_CHARS 个字：整句一次生成，不拆。
    2. 更长的：只在句号、问号、感叹号、分号、省略号、换行之后拆，
       不在逗号处拆。
    3. 每个小句至少 MIN_SEGMENT_CHARS 个字，太短的并进下一句；
       最后剩下的不足 MIN_SEGMENT_CHARS 的尾巴并进前一句。
    """

    text = text.strip()

    if not text:

        return [text]

    if len(text) <= WHOLE_SENTENCE_MAX_CHARS:

        return [text]

    parts = re.split(r"(?<=[。！？!?；;…\n])", text)

    parts = [part.strip() for part in parts if part.strip()]

    segments = []

    buffer = ""

    for part in parts:

        buffer += part

        if len(buffer) >= MIN_SEGMENT_CHARS:

            segments.append(buffer)

            buffer = ""

    if buffer:

        if segments:

            segments[-1] += buffer

        else:

            segments.append(buffer)

    return segments or [text]

def create_task(text, segments):

    """
    创建一个完整文本任务，并记录每个小句的独立生成状态。
    队列中仍然只放一个 task_id，避免一次请求被拆成多个队列项。
    """

    task_id = uuid.uuid4().hex[:12]

    segment_jobs = []

    for index, segment_text in enumerate(segments, start=1):

        segment_jobs.append({
            "segment_index": index,
            "segment_count": len(segments),
            "text": segment_text,
            "status": "queued",
            "audio_file": None,
            "error": None,
            "started_at": None,
            "finished_at": None,
        })

    job = {
        "task_id": task_id,
        "text": text,
        "status": "queued",
        "created_at": now_string(),
        "started_at": None,
        "finished_at": None,
        # 兼容旧接口：audio_file 指向第一个小句的 WAV。
        "audio_file": None,
        "error": None,
        "segment_count": len(segment_jobs),
        "segments": segment_jobs,
    }

    with jobs_lock:
        jobs[task_id] = job

    return task_id


# ============================================================
# CosyVoice 初始化
# ============================================================

def initialize_cosyvoice():

    """
    加载 CosyVoice 模型、注册声音并预热。
    """

    global cv
    global SAMPLE_RATE

    logger.info(
        "=" * 60
    )

    logger.info(
        "Initializing CosyVoice..."
    )

    logger.info(
        "=" * 60
    )

    # --------------------------------------------------------
    # 路径信息
    # --------------------------------------------------------

    logger.info(
        "Server directory: %s",
        SERVER_DIR
    )

    logger.info(
        "Project directory: %s",
        PROJECT_DIR
    )

    logger.info(
        "CosyVoice directory: %s",
        COSYVOICE_DIR
    )

    # --------------------------------------------------------
    # 检查文件
    # --------------------------------------------------------

    if not os.path.isdir(
        MODEL_DIR
    ):

        raise FileNotFoundError(
            f"找不到 CosyVoice 模型目录:\n"
            f"{MODEL_DIR}"
        )

    if not os.path.isfile(
        REF_WAV
    ):

        raise FileNotFoundError(
            f"找不到参考音频:\n"
            f"{REF_WAV}"
        )

    if not os.path.isfile(
        PROMPT_TEXT_FILE
    ):

        raise FileNotFoundError(
            f"找不到参考文本:\n"
            f"{PROMPT_TEXT_FILE}"
        )

    logger.info(
        "Model directory: %s",
        MODEL_DIR
    )

    logger.info(
        "Reference WAV: %s",
        REF_WAV
    )

    logger.info(
        "Prompt text: %s",
        PROMPT_TEXT_FILE
    )

    # --------------------------------------------------------
    # 加载模型
    # --------------------------------------------------------

    logger.info(
        "Loading CosyVoice2..."
    )

    if USE_FP16:

        try:

            cv = CosyVoice2(
                MODEL_DIR,
                load_jit=False,
                load_trt=False,
                fp16=True
            )

        except TypeError:

            logger.warning(
                "Current CosyVoice2 does not "
                "accept fp16=True. "
                "Retrying without fp16 parameter."
            )

            cv = CosyVoice2(
                MODEL_DIR,
                load_jit=False,
                load_trt=False
            )

    else:

        cv = CosyVoice2(
            MODEL_DIR,
            load_jit=False,
            load_trt=False
        )

    SAMPLE_RATE = cv.sample_rate

    logger.info(
        "CosyVoice loaded."
    )

    logger.info(
        "Sample rate: %s",
        SAMPLE_RATE
    )

    # --------------------------------------------------------
    # 读取参考文字
    # --------------------------------------------------------

    prompt_text = read_prompt_text()

    logger.info(
        "Prompt text loaded: %s",
        prompt_text
    )

    # --------------------------------------------------------
    # 注册 zero-shot speaker
    # --------------------------------------------------------

    logger.info(
        "Registering zero-shot speaker: %s",
        SPK_ID
    )

    cv.add_zero_shot_spk(
        prompt_text,
        REF_WAV,
        SPK_ID
    )

    logger.info(
        "Speaker registered."
    )

    # --------------------------------------------------------
    # 预热
    # --------------------------------------------------------

    logger.info(
        "Warming up CosyVoice..."
    )

    with inference_lock:

        # 【已修改】调用方式：
        # 前三个位置参数是 (文本, 参考文字, 参考音频)，
        # 使用已注册的音色时，后两个传空字符串，
        # 并通过 zero_shot_spk_id 指定音色。

        warmup_result = (
            cv.inference_zero_shot(
                "你好。",
                "",
                "",
                zero_shot_spk_id=SPK_ID,
                stream=False
            )
        )

        # 强制把 generator 消耗完

        for _ in warmup_result:

            pass

    logger.info(
        "CosyVoice warmup completed."
    )

    logger.info(
        "=" * 60
    )

    logger.info(
        "CosyVoice is READY."
    )

    logger.info(
        "=" * 60
    )


# ============================================================
# TTS Worker
# ============================================================

def tts_worker():

    """
    后台 TTS 工作线程。

    每个队列任务代表一整段文本；Worker 按小句顺序逐个调用 CosyVoice，
    每个小句保存为独立 WAV。服务器只负责生成，不负责播放。
    """

    global worker_running

    worker_running = True

    logger.info("[TTS Worker] Started.")

    while worker_running:

        task_id = tts_queue.get()

        if task_id is None:
            tts_queue.task_done()
            logger.info("[TTS Worker] Shutdown signal received.")
            break

        try:

            with jobs_lock:
                job = jobs.get(task_id)
                if job is None:
                    logger.error("[TTS Worker] Unknown task: %s", task_id)
                    continue

                job["status"] = "processing"
                job["started_at"] = now_string()
                original_text = job["text"]
                segment_count = len(job["segments"])

            logger.info(
                "[TTS Worker] Processing %s (%d segments): %s",
                task_id, segment_count, original_text
            )

            any_segment_failed = False

            for segment in range(1, segment_count + 1):

                with jobs_lock:
                    job = jobs[task_id]
                    segment_job = job["segments"][segment - 1]
                    segment_text = segment_job["text"]
                    segment_job["status"] = "processing"
                    segment_job["started_at"] = now_string()

                output_filename = (
                    f"{task_id}_part_{segment:02d}_of_{segment_count:02d}.wav"
                )
                output_path = os.path.join(OUTPUT_DIR, output_filename)

                logger.info(
                    "[TTS Worker] Segment %d/%d for %s: %s",
                    segment, segment_count, task_id, segment_text
                )

                try:
                    pcm_data = bytearray()

                    with inference_lock:
                        results = cv.inference_zero_shot(
                            segment_text,
                            "",
                            "",
                            zero_shot_spk_id=SPK_ID,
                            stream=False
                        )

                        for result in results:
                            audio_tensor = result["tts_speech"]
                            pcm_data.extend(tensor_to_pcm(audio_tensor))

                    if not pcm_data:
                        raise RuntimeError("CosyVoice 没有生成音频数据")

                    save_wav(output_path, bytes(pcm_data), SAMPLE_RATE)

                    with jobs_lock:
                        job = jobs[task_id]
                        segment_job = job["segments"][segment - 1]
                        segment_job["status"] = "completed"
                        segment_job["finished_at"] = now_string()
                        segment_job["audio_file"] = output_path

                        if segment == 1:
                            job["audio_file"] = output_path

                    logger.info(
                        "[TTS Worker] Completed %s segment %d/%d: %s",
                        task_id, segment, segment_count, output_filename
                    )

                except Exception as segment_error:
                    any_segment_failed = True

                    logger.exception(
                        "[TTS Worker] Failed %s segment %d/%d",
                        task_id, segment, segment_count
                    )

                    with jobs_lock:
                        job = jobs.get(task_id)
                        if job is not None:
                            segment_job = job["segments"][segment - 1]
                            segment_job["status"] = "failed"
                            segment_job["finished_at"] = now_string()
                            segment_job["error"] = str(segment_error)

            with jobs_lock:
                job = jobs.get(task_id)
                if job is not None:
                    job["status"] = "failed" if any_segment_failed else "completed"
                    job["finished_at"] = now_string()
                    if any_segment_failed:
                        job["error"] = "一个或多个小句生成失败，请检查 segments 中各小句的状态。"

            logger.info(
                "[TTS Worker] Finished %s with status=%s",
                task_id,
                "failed" if any_segment_failed else "completed"
            )

        except Exception as e:

            logger.exception("[TTS Worker] Failed task %s", task_id)

            with jobs_lock:
                job = jobs.get(task_id)
                if job is not None:
                    job["status"] = "failed"
                    job["finished_at"] = now_string()
                    job["error"] = str(e)

        finally:
            tts_queue.task_done()

    worker_running = False
    logger.info("[TTS Worker] Stopped.")


# ============================================================
# 启动 Worker
# ============================================================

def start_worker():

    global worker_thread

    if (
        worker_thread is not None
        and worker_thread.is_alive()
    ):

        return

    worker_thread = threading.Thread(
        target=tts_worker,
        name="Ratuber-TTS-Worker",
        daemon=True
    )

    worker_thread.start()

    logger.info(
        "TTS worker thread started."
    )


# ============================================================
# 健康检查
# ============================================================

@app.get("/health")
def health():

    return {

        "ok": cv is not None,

        "service":
            "Ratuber CosyVoice TTS",

        "port":
            PORT,

        "sample_rate":
            SAMPLE_RATE,

        "speaker":
            SPK_ID,

        "queue_size":
            tts_queue.qsize(),

        "worker_alive": (
            worker_thread is not None
            and worker_thread.is_alive()
        ),

        "model":
            "CosyVoice2-0.5B",
    }


# ============================================================
# 提交 TTS
# ============================================================

@app.post("/tts")
def submit_tts(
    request: TTSRequest
):

    text = request.text.strip()

    # --------------------------------------------------------
    # 参数检查
    # --------------------------------------------------------

    if not text:

        raise HTTPException(
            status_code=400,
            detail="text 不能为空"
        )

    if len(text) > MAX_CHARS:

        raise HTTPException(
            status_code=400,
            detail=(
                f"text 太长，最多 "
                f"{MAX_CHARS} 个字符"
            )
        )

    # --------------------------------------------------------
    # 检查服务器
    # --------------------------------------------------------

    if cv is None:

        raise HTTPException(
            status_code=503,
            detail="CosyVoice 尚未准备完成"
        )

    if (
        worker_thread is None
        or not worker_thread.is_alive()
    ):

        raise HTTPException(
            status_code=503,
            detail="TTS Worker 未运行"
        )

    # --------------------------------------------------------
    # 自动拆分小句并创建整段任务
    # --------------------------------------------------------

    segments = split_text_into_segments(text)

    task_id = create_task(
        text,
        segments
    )

    # --------------------------------------------------------
    # 放入 FIFO 队列
    # --------------------------------------------------------

    try:

        tts_queue.put_nowait(
            task_id
        )

    except queue.Full:

        with jobs_lock:

            jobs.pop(
                task_id,
                None
            )

        raise HTTPException(
            status_code=429,
            detail=(
                "TTS 队列已满，"
                "请稍后再试"
            )
        )

    logger.info(
        "[API] Queued %s (%d segments): %s",
        task_id,
        len(segments),
        text
    )

    # --------------------------------------------------------
    # 关键：
    #
    # 这里绝对不会等待 CosyVoice。
    # --------------------------------------------------------

    return {

        "ok": True,

        "task_id":
            task_id,

        "status":
            "queued",

        "text":
            text,

        "segment_count":
            len(segments),

        "segments": [
            {
                "segment_index": index,
                "segment_count": len(segments),
                "text": segment_text,
                "status": "queued",
                "filename": (
                    f"{task_id}_part_{index:02d}_of_{len(segments):02d}.wav"
                ),
                "audio_url": f"/audio/{task_id}/{index}",
            }
            for index, segment_text in enumerate(segments, start=1)
        ],

        "queue_size":
            tts_queue.qsize(),

        "status_url":
            f"/tts/{task_id}",

        # 兼容旧接口：/audio/{task_id} 获取第一个小句。
        "audio_url":
            f"/audio/{task_id}",
    }


# ============================================================
# 查询 TTS 状态
# ============================================================

@app.get(
    "/tts/{task_id}"
)
def get_tts_status(
    task_id: str
):

    with jobs_lock:
        job = jobs.get(task_id)

        if job is None:
            raise HTTPException(
                status_code=404,
                detail="找不到这个 task_id"
            )

        result = dict(job)
        result.pop("audio_file", None)

        # 不向客户端暴露服务器内部完整路径。
        public_segments = []
        for segment in job["segments"]:
            public_segment = dict(segment)
            audio_file = public_segment.pop("audio_file", None)
            public_segment["filename"] = (
                os.path.basename(audio_file)
                if audio_file
                else f"{task_id}_part_{public_segment['segment_index']:02d}_of_{public_segment['segment_count']:02d}.wav"
            )
            if public_segment["status"] == "completed":
                public_segment["audio_url"] = (
                    f"/audio/{task_id}/{public_segment['segment_index']}"
                )
            else:
                public_segment["audio_url"] = None
            public_segments.append(public_segment)

        result["segments"] = public_segments

        # 保留旧字段，指向第一个小句；新客户端应优先读取 segments。
        first_segment = job["segments"][0] if job["segments"] else None
        if first_segment and first_segment["status"] == "completed":
            result["audio_url"] = f"/audio/{task_id}"

    return result


def _get_segment_audio(task_id: str, segment_index: int):

    with jobs_lock:
        job = jobs.get(task_id)

        if job is None:
            raise HTTPException(status_code=404, detail="找不到这个 task_id")

        segments = job.get("segments", [])

        if segment_index < 1 or segment_index > len(segments):
            raise HTTPException(
                status_code=404,
                detail=f"小句序号无效，有效范围为 1 到 {len(segments)}"
            )

        segment = dict(segments[segment_index - 1])
        status = segment["status"]
        audio_file = segment["audio_file"]

    if status == "queued":
        raise HTTPException(status_code=409, detail="这个小句还在队列中")

    if status == "processing":
        raise HTTPException(status_code=409, detail="这个小句正在生成")

    if status == "failed":
        raise HTTPException(status_code=500, detail="这个小句生成失败")

    if not audio_file:
        raise HTTPException(status_code=500, detail="小句状态已完成但找不到 WAV 文件")

    if not os.path.isfile(audio_file):
        raise HTTPException(status_code=404, detail="WAV 文件不存在")

    return FileResponse(
        audio_file,
        media_type="audio/wav",
        filename=os.path.basename(audio_file)
    )


# ============================================================
# 获取 WAV
# ============================================================

@app.get("/audio/{task_id}")
def get_audio(task_id: str):

    # 兼容旧接口：默认获取整段文本的第一个小句。
    return _get_segment_audio(task_id, 1)


@app.get("/audio/{task_id}/{segment_index}")
def get_segment_audio(task_id: str, segment_index: int):

    # segment_index 从 1 开始，例如 2 表示整段文本的第 2 个小句。
    return _get_segment_audio(task_id, segment_index)


# ============================================================
# 服务关闭
# ============================================================

@app.on_event(
    "shutdown"
)
def shutdown_event():

    global worker_running

    logger.info(
        "Shutting down TTS server..."
    )

    worker_running = False

    # 给 worker 发送退出信号

    try:

        tts_queue.put_nowait(
            None
        )

    except queue.Full:

        logger.warning(
            "TTS queue is full "
            "during shutdown."
        )

    if (
        worker_thread is not None
        and worker_thread.is_alive()
    ):

        worker_thread.join(
            timeout=5
        )

    logger.info(
        "TTS server shutdown complete."
    )


# ============================================================
# 主程序
# ============================================================

def main():

    logger.info("")

    logger.info(
        "=============================================="
    )

    logger.info(
        " Ratuber CosyVoice Async TTS Server"
    )

    logger.info(
        "=============================================="
    )

    logger.info(
        "Server directory: %s",
        SERVER_DIR
    )

    logger.info(
        "Project directory: %s",
        PROJECT_DIR
    )

    logger.info(
        "CosyVoice directory: %s",
        COSYVOICE_DIR
    )

    logger.info(
        "Model directory: %s",
        MODEL_DIR
    )

    logger.info(
        "Output directory: %s",
        OUTPUT_DIR
    )

    # --------------------------------------------------------
    # 加载 CosyVoice
    # --------------------------------------------------------

    initialize_cosyvoice()

    # --------------------------------------------------------
    # 启动后台 Worker
    # --------------------------------------------------------

    start_worker()

    # --------------------------------------------------------
    # 启动 FastAPI
    # --------------------------------------------------------

    logger.info(
        "Starting FastAPI..."
    )

    logger.info(
        "Server: http://%s:%d",
        HOST,
        PORT
    )

    logger.info(
        "Health: http://%s:%d/health",
        HOST,
        PORT
    )

    uvicorn.run(
        app,
        host=HOST,
        port=PORT,
        log_level="info"
    )


# ============================================================
# Entry Point
# ============================================================

if __name__ == "__main__":

    main()