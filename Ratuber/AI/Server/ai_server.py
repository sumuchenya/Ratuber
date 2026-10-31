from pathlib import Path
from typing import Optional, Literal

import json
import random
import uvicorn

from fastapi import FastAPI, HTTPException
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field


# ============================================================
# Paths
# ============================================================

SERVER_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SERVER_DIR.parents[1]

BEHAVIOR_FILE = PROJECT_DIR / "behavior" / "behaviors.json"


# ============================================================
# FastAPI
# ============================================================

app = FastAPI(
    title="Ratuber AI Server",
    description="Ratuber AI / Behavior Decision Server",
    version="2.0.0",
)


# ============================================================
# Data Models
# ============================================================

class ClientState(BaseModel):
    """
    客户端当前状态。

    busy = true:
        当前客户端正在播放一个动作。
        服务端会直接降级到 L3，只发送 expression。
    """

    busy: bool = False


class DecideRequest(BaseModel):
    """
    /decide 请求。

    emotion:
        当前回复对应的情绪。
        以后由 LLM 提供。

    max_level:
        服务端允许使用的最高等级。

        compose:
            允许 L1 -> L2 -> L3

        preset:
            允许 L2 -> L3

        atom:
            只允许 L3

    client_state:
        客户端当前状态。

        如果客户端没有提供：
            busy = false
    """

    text: str = ""

    emotion: Optional[str] = None

    max_level: Literal[
        "compose",
        "preset",
        "atom"
    ] = "compose"

    client_state: ClientState = Field(
        default_factory=ClientState
    )


# ============================================================
# Behavior Registry
# ============================================================

behavior_registry = {
    "motion": [],
    "expression": [],
    "combination": [],
}

emotion_pools = {}


# ============================================================
# Previous Result Memory
#
# 用于避免同一个 emotion 连续抽到完全相同的
# motion + expression。
# ============================================================

last_composed_result = {}


# ============================================================
# Utility
# ============================================================

def weighted_choice(items: list[dict]) -> Optional[dict]:
    """
    按 weight 进行加权随机。

    items:
    [
        {
            "id": "...",
            "weight": 3
        }
    ]
    """

    if not items:
        return None

    valid_items = []

    for item in items:
        if not isinstance(item, dict):
            continue

        item_id = item.get("id")

        if not item_id:
            continue

        try:
            weight = float(item.get("weight", 1))
        except (TypeError, ValueError):
            weight = 1

        if weight <= 0:
            continue

        valid_items.append(
            (
                item,
                weight
            )
        )

    if not valid_items:
        return None

    items_only = [
        item
        for item, _ in valid_items
    ]

    weights = [
        weight
        for _, weight in valid_items
    ]

    return random.choices(
        items_only,
        weights=weights,
        k=1
    )[0]


# ============================================================
# Registry Loading
# ============================================================

def load_behavior_registry():
    """
    从 behaviors.json 加载：

        behaviors.motion
        behaviors.expression
        behaviors.combination
        emotion_pools

    """

    global behavior_registry
    global emotion_pools

    if not BEHAVIOR_FILE.exists():
        raise FileNotFoundError(
            f"Behavior registry not found: {BEHAVIOR_FILE}"
        )

    with open(
        BEHAVIOR_FILE,
        "r",
        encoding="utf-8"
    ) as f:
        data = json.load(f)

    behaviors = data.get(
        "behaviors",
        {}
    )

    behavior_registry = {
        "motion": behaviors.get(
            "motion",
            []
        ),
        "expression": behaviors.get(
            "expression",
            []
        ),
        "combination": behaviors.get(
            "combination",
            []
        ),
    }

    emotion_pools = data.get(
        "emotion_pools",
        {}
    )

    print()
    print("========================================")
    print("       [AI Server] Behavior Registry Loaded")
    print("========================================")
    print(f"[AI Server] File: {BEHAVIOR_FILE}")

    print(
        f"[AI Server] Motion behaviors: "
        f"{len(behavior_registry['motion'])}"
    )

    print(
        f"[AI Server] Expression behaviors: "
        f"{len(behavior_registry['expression'])}"
    )

    print(
        f"[AI Server] Combination behaviors: "
        f"{len(behavior_registry['combination'])}"
    )

    print(
        f"[AI Server] Emotion pools: "
        f"{len(emotion_pools)}"
    )

    print("========================================")
    print()


# ============================================================
# Find Behavior
# ============================================================

def find_behavior(
    behavior_id: str,
    behavior_type: Optional[str] = None
) -> Optional[dict]:
    """
    根据 ID 查找行为。

    如果 behavior_type 指定：
        只搜索指定类型。

    如果没有指定：
        搜索 motion / expression / combination。
    """

    if behavior_type:
        behavior_types = [
            behavior_type
        ]
    else:
        behavior_types = [
            "motion",
            "expression",
            "combination",
        ]

    for current_type in behavior_types:

        for behavior in behavior_registry.get(
            current_type,
            []
        ):

            if behavior.get("id") == behavior_id:
                return behavior

    return None


# ============================================================
# Find Combination
# ============================================================

def find_combination(
    emotion: str
) -> Optional[dict]:
    """
    找到 emotion 对应的预设 CombinationBehavior。

    匹配顺序：
        1. 先找 id 完全等于 emotion 的组合
        2. 找不到，再按 emotion 别名列表匹配

    这样 happy 会命中 id 为 happy 的组合，
    而不是 emotion 列表里恰好包含 "happy" 的 greeting。
    """

    combinations = behavior_registry.get(
        "combination",
        []
    )

    # --------------------------------------------------------
    # 第一轮：id 精确匹配优先
    # --------------------------------------------------------

    for behavior in combinations:

        if behavior.get("id") == emotion:
            return behavior

    # --------------------------------------------------------
    # 第二轮：别名匹配
    # --------------------------------------------------------

    for behavior in combinations:

        emotions = behavior.get(
            "emotion",
            []
        )

        if emotion in emotions:
            return behavior

    return None


# ============================================================
# Get Emotion Pool
# ============================================================

def get_emotion_pool(
    emotion: str
) -> Optional[dict]:

    pool = emotion_pools.get(
        emotion
    )

    if not isinstance(pool, dict):
        return None

    return pool


# ============================================================
# Validate Expression
# ============================================================

def is_valid_expression(
    expression_id: str
) -> bool:

    return (
        find_behavior(
            expression_id,
            "expression"
        )
        is not None
    )


# ============================================================
# Validate Motion
# ============================================================

def get_motion(
    motion_id: str
) -> Optional[dict]:

    return find_behavior(
        motion_id,
        "motion"
    )


# ============================================================
# Compatible Expression
# ============================================================

def is_expression_compatible(
    motion: dict,
    expression_id: str
) -> bool:

    compatible = motion.get(
        "compatible_expressions",
        []
    )

    return expression_id in compatible


# ============================================================
# Build Valid Expression Pool
# ============================================================

def get_valid_expression_candidates(
    motion: dict,
    emotion_pool: dict
) -> list[dict]:

    expression_pool = emotion_pool.get(
        "expressions",
        []
    )

    valid = []

    for expression_item in expression_pool:

        if not isinstance(
            expression_item,
            dict
        ):
            continue

        expression_id = expression_item.get(
            "id"
        )

        if not expression_id:
            continue

        # expression 必须真实存在
        if not is_valid_expression(
            expression_id
        ):
            continue

        # expression 必须在 motion
        # 的 compatible_expressions 中
        if not is_expression_compatible(
            motion,
            expression_id
        ):
            continue

        valid.append(
            expression_item
        )

    return valid


# ============================================================
# L1 - Random Composition
# ============================================================

def try_compose(
    emotion: str
) -> Optional[dict]:

    pool = get_emotion_pool(
        emotion
    )

    if pool is None:
        return None

    motion_pool = pool.get(
        "motions",
        []
    )

    expression_pool = pool.get(
        "expressions",
        []
    )

    if not motion_pool:
        return None

    if not expression_pool:
        return None

    # --------------------------------------------------------
    # 构建候选：每个合法 motion + 它可用的 expression 列表
    # --------------------------------------------------------

    candidates = []

    for motion_item in motion_pool:

        if not isinstance(
            motion_item,
            dict
        ):
            continue

        motion_id = motion_item.get(
            "id"
        )

        if not motion_id:
            continue

        motion = get_motion(
            motion_id
        )

        if motion is None:
            continue

        valid_expressions = (
            get_valid_expression_candidates(
                motion,
                pool
            )
        )

        if not valid_expressions:
            continue

        candidates.append(
            {
                "motion_item": motion_item,
                "motion": motion,
                "expressions": valid_expressions,
            }
        )

    if not candidates:
        return None

    # --------------------------------------------------------
    # 避免连续出现完全一样的 (motion, expression)。
    #
    # 对每个 motion 单独剔除"和上次完全相同"的 expression：
    #   - 该 motion 还剩 expression：保留，且只保留剩下的
    #   - 该 motion 一个都不剩：整个 motion 丢掉
    #
    # 如果剔除后一个候选都没有（说明这个 emotion 只有
    # 唯一一种合法组合），就保持原样，允许重复。
    # --------------------------------------------------------

    last_result = last_composed_result.get(
        emotion
    )

    if last_result is not None:

        filtered_candidates = []

        for candidate in candidates:

            motion_id = candidate[
                "motion_item"
            ].get("id")

            remaining_expressions = []

            for expression_item in candidate[
                "expressions"
            ]:

                expression_id = expression_item.get(
                    "id"
                )

                if (
                    motion_id,
                    expression_id
                ) != last_result:

                    remaining_expressions.append(
                        expression_item
                    )

            if remaining_expressions:

                filtered_candidates.append(
                    {
                        "motion_item": candidate[
                            "motion_item"
                        ],
                        "motion": candidate[
                            "motion"
                        ],
                        "expressions": remaining_expressions,
                    }
                )

        if filtered_candidates:
            candidates = filtered_candidates

    # --------------------------------------------------------
    # 按 motion weight 选择
    # --------------------------------------------------------

    weighted_motions = [
        candidate["motion_item"]
        for candidate in candidates
    ]

    selected_motion_item = weighted_choice(
        weighted_motions
    )

    if selected_motion_item is None:
        return None

    selected_motion_id = selected_motion_item.get(
        "id"
    )

    selected_candidate = None

    for candidate in candidates:

        if candidate[
            "motion_item"
        ].get("id") == selected_motion_id:

            selected_candidate = candidate
            break

    if selected_candidate is None:
        return None

    # --------------------------------------------------------
    # 按 expression weight 选择
    # --------------------------------------------------------

    selected_expression_item = weighted_choice(
        selected_candidate[
            "expressions"
        ]
    )

    if selected_expression_item is None:
        return None

    selected_expression_id = (
        selected_expression_item.get(
            "id"
        )
    )

    # --------------------------------------------------------
    # 最终再次验证
    # --------------------------------------------------------

    motion = selected_candidate[
        "motion"
    ]

    if not is_expression_compatible(
        motion,
        selected_expression_id
    ):
        return None

    # --------------------------------------------------------
    # 保存上一次组合
    # --------------------------------------------------------

    last_composed_result[
        emotion
    ] = (
        selected_motion_id,
        selected_expression_id
    )

    return {
        "motion": selected_motion_id,
        "expression": selected_expression_id,
    }


# ============================================================
# L2 - Preset Combination
# ============================================================

def try_preset(
    emotion: str
) -> Optional[dict]:

    combination = find_combination(
        emotion
    )

    if combination is None:
        return None

    components = combination.get(
        "components",
        []
    )

    if not components:
        return None

    motion_id = None
    expression_id = None

    # --------------------------------------------------------
    # 从 CombinationBehavior 中找 motion / expression
    # --------------------------------------------------------

    for component_id in components:

        if find_behavior(
            component_id,
            "motion"
        ) is not None:

            motion_id = component_id

        elif find_behavior(
            component_id,
            "expression"
        ) is not None:

            expression_id = component_id

    # --------------------------------------------------------
    # L2 必须是一个合法的预设组合。
    #
    # 这里允许只有 expression 的组合，
    # 例如 sad。
    # --------------------------------------------------------

    if motion_id is not None:

        motion = get_motion(
            motion_id
        )

        if motion is None:
            return None

        if expression_id is not None:

            if not is_expression_compatible(
                motion,
                expression_id
            ):
                return None

    if motion_id is None and expression_id is None:
        return None

    return {
        "motion": motion_id,
        "expression": expression_id,
    }


# ============================================================
# L3 - Expression Only
# ============================================================

def try_atom(
    emotion: str
) -> Optional[dict]:

    pool = get_emotion_pool(
        emotion
    )

    if pool is None:
        return None

    expression_pool = pool.get(
        "expressions",
        []
    )

    if not expression_pool:
        return None

    valid_expressions = []

    for expression_item in expression_pool:

        if not isinstance(
            expression_item,
            dict
        ):
            continue

        expression_id = expression_item.get(
            "id"
        )

        if not expression_id:
            continue

        if not is_valid_expression(
            expression_id
        ):
            continue

        valid_expressions.append(
            expression_item
        )

    if not valid_expressions:
        return None

    selected = weighted_choice(
        valid_expressions
    )

    if selected is None:
        return None

    return {
        "motion": None,
        "expression": selected.get(
            "id"
        ),
    }


# ============================================================
# L0 - Idle
# ============================================================

def build_idle_result(
    emotion: Optional[str],
    reason: str
) -> dict:

    return {
        "success": True,
        "emotion": emotion,
        "level": "idle",
        "reason": reason,
        "actions": {
            "motion": None,
            "expression": None,
        },
    }


# ============================================================
# Decide Behavior
# ============================================================

def decide_behavior(
    emotion: Optional[str],
    max_level: str,
    busy: bool
) -> dict:

    # ========================================================
    # L0
    #
    # emotion 缺失
    # ========================================================

    if not emotion:

        return build_idle_result(
            emotion=None,
            reason="emotion_missing"
        )

    emotion = emotion.strip()

    if not emotion:

        return build_idle_result(
            emotion=None,
            reason="emotion_missing"
        )

    # ========================================================
    # 检查 emotion 是否存在
    #
    # emotion 必须至少存在于：
    #
    # emotion_pools
    #
    # 或
    #
    # CombinationBehavior
    #
    # 否则不猜。
    # ========================================================

    pool_exists = (
        emotion in emotion_pools
    )

    combination_exists = (
        find_combination(
            emotion
        )
        is not None
    )

    if (
        not pool_exists
        and not combination_exists
    ):

        return build_idle_result(
            emotion=emotion,
            reason="invalid_emotion"
        )

    # ========================================================
    # 客户端 BUSY
    #
    # busy 是强制降级。
    #
    # 不允许：
    # L1
    # L2
    #
    # 直接 L3。
    # ========================================================

    if busy:

        atom_result = try_atom(
            emotion
        )

        if atom_result is not None:

            return {
                "success": True,
                "emotion": emotion,
                "level": "atom",
                "reason": "client_busy",
                "actions": atom_result,
            }

        return build_idle_result(
            emotion=emotion,
            reason="client_busy_no_expression"
        )

    # ========================================================
    # max_level = atom
    # ========================================================

    if max_level == "atom":

        atom_result = try_atom(
            emotion
        )

        if atom_result is not None:

            return {
                "success": True,
                "emotion": emotion,
                "level": "atom",
                "reason": "max_level_atom",
                "actions": atom_result,
            }

        return build_idle_result(
            emotion=emotion,
            reason="atom_unavailable"
        )

    # ========================================================
    # max_level = preset
    #
    # L2 -> L3
    # ========================================================

    if max_level == "preset":

        preset_result = try_preset(
            emotion
        )

        if preset_result is not None:

            return {
                "success": True,
                "emotion": emotion,
                "level": "preset",
                "reason": "max_level_preset",
                "behavior": emotion,
                "actions": preset_result,
            }

        atom_result = try_atom(
            emotion
        )

        if atom_result is not None:

            return {
                "success": True,
                "emotion": emotion,
                "level": "atom",
                "reason": "preset_failed",
                "actions": atom_result,
            }

        return build_idle_result(
            emotion=emotion,
            reason="preset_and_atom_failed"
        )

    # ========================================================
    # max_level = compose
    #
    # L1 -> L2 -> L3
    # ========================================================

    compose_result = try_compose(
        emotion
    )

    if compose_result is not None:

        return {
            "success": True,
            "emotion": emotion,
            "level": "compose",
            "reason": "compose_success",
            "actions": compose_result,
        }

    # ========================================================
    # L1 失败
    # ↓
    # L2
    # ========================================================

    preset_result = try_preset(
        emotion
    )

    if preset_result is not None:

        return {
            "success": True,
            "emotion": emotion,
            "level": "preset",
            "reason": "compose_failed",
            "behavior": emotion,
            "actions": preset_result,
        }

    # ========================================================
    # L2 失败
    # ↓
    # L3
    # ========================================================

    atom_result = try_atom(
        emotion
    )

    if atom_result is not None:

        return {
            "success": True,
            "emotion": emotion,
            "level": "atom",
            "reason": "compose_and_preset_failed",
            "actions": atom_result,
        }

    # ========================================================
    # L3 失败
    # ↓
    # L0
    # ========================================================

    return build_idle_result(
        emotion=emotion,
        reason="compose_preset_atom_failed"
    )


# ============================================================
# Startup
# ============================================================

@app.on_event("startup")
def startup_event():

    load_behavior_registry()


# ============================================================
# Health
# ============================================================

@app.get("/health")
def health():

    return JSONResponse(
        content={
            "status": "ok",
            "server": "Ratuber AI Server",
            "port": 8001,
        },
        media_type="application/json; charset=utf-8",
    )


# ============================================================
# All Behaviors
# ============================================================

@app.get("/behaviors")
def get_behaviors():

    return JSONResponse(
        content={
            "success": True,
            "behaviors": behavior_registry,
            "emotion_pools": emotion_pools,
        },
        media_type="application/json; charset=utf-8",
    )


# ============================================================
# Combination Behaviors
# ============================================================

@app.get("/behaviors/combination")
def get_combination_behaviors():

    return JSONResponse(
        content={
            "success": True,
            "behaviors": behavior_registry.get(
                "combination",
                []
            ),
        },
        media_type="application/json; charset=utf-8",
    )


# ============================================================
# Single Behavior
# ============================================================

@app.get("/behaviors/{behavior_id}")
def get_behavior(
    behavior_id: str
):

    behavior = find_behavior(
        behavior_id
    )

    if behavior is None:

        raise HTTPException(
            status_code=404,
            detail=(
                f"Behavior not found: "
                f"{behavior_id}"
            ),
        )

    return JSONResponse(
        content={
            "success": True,
            "behavior": behavior,
        },
        media_type="application/json; charset=utf-8",
    )


# ============================================================
# Manual Behavior Test
# ============================================================

class BehaviorRequest(BaseModel):
    behavior: str


@app.post("/behavior")
def execute_behavior(
    request: BehaviorRequest
):

    behavior = find_behavior(
        request.behavior
    )

    if behavior is None:

        raise HTTPException(
            status_code=404,
            detail=(
                f"Behavior not found: "
                f"{request.behavior}"
            ),
        )

    return JSONResponse(
        content={
            "success": True,
            "behavior": behavior,
        },
        media_type="application/json; charset=utf-8",
    )


# ============================================================
# AI Decision
# ============================================================

@app.post("/decide")
def decide(
    request: DecideRequest
):

    # --------------------------------------------------------
    # text 现在只是保留给未来 LLM / 日志使用。
    #
    # 服务器不再通过 text 猜 emotion。
    # --------------------------------------------------------

    text = request.text.strip()

    result = decide_behavior(
        emotion=request.emotion,
        max_level=request.max_level,
        busy=request.client_state.busy,
    )

    # --------------------------------------------------------
    # 保留输入信息，方便调试。
    # --------------------------------------------------------

    result["input"] = {
        "text": text,
        "emotion": request.emotion,
        "max_level": request.max_level,
        "client_busy": request.client_state.busy,
    }

    return JSONResponse(
        content=result,
        media_type="application/json; charset=utf-8",
    )


# ============================================================
# Main
# ============================================================

if __name__ == "__main__":

    print()
    print("========================================")
    print("       Ratuber AI Server")
    print("========================================")
    print(
        "Address : "
        "http://127.0.0.1:8001"
    )
    print(
        "Health  : "
        "http://127.0.0.1:8001/health"
    )
    print(
        "Behaviors:")
    print(
        "         "
        "http://127.0.0.1:8001/behaviors"
    )
    print(
        "Decide:")
    print(
        "         "
        "POST http://127.0.0.1:8001/decide"
    )
    print("========================================")
    print()

    uvicorn.run(
        app,
        host="127.0.0.1",
        port=8001,
    )