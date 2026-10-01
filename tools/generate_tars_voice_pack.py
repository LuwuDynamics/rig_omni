#!/usr/bin/env python3
"""Generate the TARS voice-prompt audition pack with Volcengine Doubao TTS 2.0.

The script uses the current V3 SSE endpoint and asks for Ogg Opus at 16 kHz,
which is directly compatible with RIG-Omni's 16 kHz audio pipeline.  It does
not store credentials: pass the speech API key and cloned-speaker ID through
environment variables or command-line options.

Required configuration:
    export VOLC_SPEECH_API_KEY='...'
    export VOLC_TTS_SPEAKER='your-cloned-TARS-speaker-id'

Example:
    python3 tools/generate_tars_voice_pack.py
    python3 tools/generate_tars_voice_pack.py --only tars_hi,wificonfig

Get the API key from the Volcengine Speech console's "API Key" page.  The
speaker ID is the cloned TARS timbre's API speaker/voice ID, not its display
name ("TARS").
"""

from __future__ import annotations

import argparse
import base64
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
from uuid import uuid4


ENDPOINT = "https://openspeech.bytedance.com/api/v3/tts/unidirectional/sse"
# S_* speaker IDs are Doubao ICL voice-cloning 2.0 timbres.  They must use
# seed-icl-2.0; seed-tts-2.0 only accepts the ordinary TTS 2.0 speakers.
DEFAULT_RESOURCE_ID = "seed-icl-2.0"
PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "tools" / "tars_voice_output"

# Keep each line separate: firmware cues should be short, immediately
# recognizable, and easy to replace after listening to a single result.
VOICE_LINES: dict[str, str] = {
    "tars_startup": "TARS online. All systems nominal.",
    "tars_hi": "I'm here.",
    "activation": "Identity verification in progress. Honesty is set to ninety percent.",
    "activation_error": "Verification failed. Bureaucracy has won this round.",
    "calibration_enter": "Calibration mode active. Precision is appreciated.",
    "calibration_exit": "Calibration complete. I appear to be mostly symmetrical.",
    "close_aec": "Echo cancellation disabled. This may become philosophically noisy.",
    "connect_error": "Network link unavailable. The universe is being uncooperative.",
    "connecting": "Network configuration active. I'll wait.",
    "enter_remote": "Remote control enabled. Please avoid creative driving.",
    "exit_remote": "Remote control disabled. I'm driving again.",
    "open_aec": "Echo cancellation enabled. I can hear myself thinking less.",
    "pain": "That was unnecessary.",
    "upgrade": "System update in progress. Please do not interrupt my improvement.",
    "welcome": "TARS standing by. Try not to miss me.",
    "wifi_success": "Connection established. That was less painful than expected.",
    "wificonfig": "Network configuration active. I'll wait.",
    "exclamation": "Interesting.",
    "low_battery": "Power reserves are low. I recommend charging before heroics.",
    # After STT accepts the user's utterance, TARS acknowledges receipt.
    "message_send": "Copy that.",
    # After TARS finishes speaking, it yields the radio channel for a reply.
    "over": "Over.",
    "popup": "Incoming message. Apparently, I'm popular.",
    "success": "Reset confirmed. I'll try not to take it personally.",
    "vibration": "Alert acknowledged.",
}


class TtsError(RuntimeError):
    """A request or response error reported by the speech API."""


def opus_input_sample_rate(audio: bytes) -> int | None:
    """Read the original input sample rate stored in an OpusHead packet."""
    header_at = audio.find(b"OpusHead")
    if header_at < 0 or len(audio) < header_at + 16:
        return None
    return int.from_bytes(audio[header_at + 12 : header_at + 16], "little")


def save_as_16khz_ogg(audio: bytes, output_path: Path) -> int:
    """Save an API response, resampling clone-model 24 kHz output if needed.

    Doubao ICL 2.0 may currently return 24 kHz Ogg Opus even when the request
    asks for 16 kHz.  Decode and re-encode only in that case; regular TTS
    responses that already carry a 16 kHz OpusHead are written unchanged.
    """
    source_rate = opus_input_sample_rate(audio)
    if source_rate == 16000:
        output_path.write_bytes(audio)
        return source_rate

    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise TtsError(
            f"{output_path.name} is {source_rate} Hz and ffmpeg is required "
            "to normalize it to 16 kHz"
        )

    source_path = output_path.with_name(f".{output_path.stem}.source.ogg")
    temp_path = output_path.with_name(f".{output_path.stem}.tmp.ogg")
    source_path.write_bytes(audio)
    try:
        subprocess.run(
            [
                ffmpeg,
                "-y",
                "-hide_banner",
                "-loglevel",
                "error",
                "-i",
                str(source_path),
                "-vn",
                "-ac",
                "1",
                "-ar",
                "16000",
                "-c:a",
                "libopus",
                "-b:a",
                "32k",
                "-vbr",
                "on",
                str(temp_path),
            ],
            check=True,
        )
        temp_path.replace(output_path)
    except subprocess.CalledProcessError as exc:
        raise TtsError(f"ffmpeg failed while converting {output_path.name}") from exc
    finally:
        source_path.unlink(missing_ok=True)
        temp_path.unlink(missing_ok=True)

    result_rate = opus_input_sample_rate(output_path.read_bytes())
    if result_rate != 16000:
        output_path.unlink(missing_ok=True)
        raise TtsError(
            f"{output_path.name} reports OpusHead sample rate {result_rate}, expected 16000"
        )
    return source_rate


def build_request_body(
    text: str,
    speaker: str,
    *,
    speech_rate: int,
    pitch: int,
    loudness_rate: int,
    model: str | None,
) -> dict[str, Any]:
    """Build a V3 request for compact firmware audio prompts."""
    req_params: dict[str, Any] = {
        "text": text,
        "speaker": speaker,
        "sample_rate": 16000,
        "audio_params": {
            "format": "ogg_opus",
            "bit_rate": 32000,
            "speech_rate": speech_rate,
            "loudness_rate": loudness_rate,
        },
        "additions": json.dumps(
            {
                "post_process": {"pitch": pitch},
                "disable_markdown_filter": True,
                "enable_latex_tn": False,
            }
        ),
    }
    if model:
        req_params["model"] = model
    return {"user": {"uid": "rig-omni-tars"}, "req_params": req_params}


def synthesize(
    *,
    api_key: str,
    resource_id: str,
    speaker: str,
    text: str,
    speech_rate: int,
    pitch: int,
    loudness_rate: int,
    model: str | None,
    timeout: float,
) -> bytes:
    """Call V3 SSE and concatenate its base64 Ogg Opus data frames."""
    body = build_request_body(
        text,
        speaker,
        speech_rate=speech_rate,
        pitch=pitch,
        loudness_rate=loudness_rate,
        model=model,
    )
    request = Request(
        ENDPOINT,
        data=json.dumps(body).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "X-Api-Key": api_key,
            "X-Api-Resource-Id": resource_id,
            "X-Api-Request-Id": str(uuid4()),
        },
        method="POST",
    )

    chunks: list[bytes] = []
    completed = False
    try:
        with urlopen(request, timeout=timeout) as response:
            for raw_line in response:
                line = raw_line.decode("utf-8", errors="replace").strip()
                if not line.startswith("data:"):
                    continue
                try:
                    payload = json.loads(line[5:].strip())
                except json.JSONDecodeError:
                    continue

                code = payload.get("code")
                if code == 0:
                    data = payload.get("data")
                    if data:
                        chunks.append(base64.b64decode(data))
                elif code == 20_000_000:
                    completed = True
                    break
                else:
                    raise TtsError(
                        f"API error code={code}: {payload.get('message', payload)}"
                    )
    except HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise TtsError(f"HTTP {exc.code}: {detail}") from exc
    except URLError as exc:
        raise TtsError(f"Network error: {exc.reason}") from exc

    if not completed:
        raise TtsError("API stream ended before the completion frame")
    audio = b"".join(chunks)
    if not audio:
        raise TtsError("API returned no audio data")
    if b"OggS" not in audio[:64] or b"OpusHead" not in audio:
        raise TtsError("API did not return an Ogg Opus stream")
    return audio


def selected_lines(value: str | None) -> dict[str, str]:
    if not value:
        return VOICE_LINES
    names = [item.strip() for item in value.split(",") if item.strip()]
    invalid = [name for name in names if name not in VOICE_LINES]
    if invalid:
        available = ", ".join(VOICE_LINES)
        raise TtsError(f"Unknown cue(s): {', '.join(invalid)}. Available: {available}")
    return {name: VOICE_LINES[name] for name in names}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--api-key",
        default=os.getenv("VOLC_SPEECH_API_KEY"),
        help="Volcengine Speech API key (default: VOLC_SPEECH_API_KEY)",
    )
    parser.add_argument(
        "--speaker",
        default=os.getenv("VOLC_TTS_SPEAKER"),
        help="Cloned TARS speaker ID (default: VOLC_TTS_SPEAKER)",
    )
    parser.add_argument(
        "--resource-id",
        default=os.getenv("VOLC_TTS_RESOURCE_ID", DEFAULT_RESOURCE_ID),
        help=(
            "Volcengine TTS resource ID (default: seed-icl-2.0 for cloned "
            "S_* speakers; override with VOLC_TTS_RESOURCE_ID if needed)"
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Output directory (default: {DEFAULT_OUTPUT_DIR})",
    )
    parser.add_argument(
        "--only",
        help="Comma-separated cue names, for example: tars_hi,wificonfig",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Regenerate files that already exist",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print selected files and text without calling the API",
    )
    parser.add_argument(
        "--speech-rate",
        type=int,
        default=-10,
        help="TTS rate adjustment; -10 keeps TARS deliberate (default: -10)",
    )
    parser.add_argument(
        "--pitch",
        type=int,
        default=-1,
        help="Post-process pitch adjustment (default: -1)",
    )
    parser.add_argument(
        "--loudness-rate",
        type=int,
        default=0,
        help="TTS loudness adjustment (default: 0)",
    )
    parser.add_argument(
        "--model",
        default=None,
        help="Optional TTS model override; omit to use the console default",
    )
    parser.add_argument("--timeout", type=float, default=60.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        lines = selected_lines(args.only)
        if not args.dry_run and not args.api_key:
            raise TtsError("Set VOLC_SPEECH_API_KEY or pass --api-key.")
        if not args.dry_run and not args.speaker:
            raise TtsError(
                "Set VOLC_TTS_SPEAKER to the cloned voice ID, not the display name."
            )

        output_dir = args.output_dir.resolve()
        for name, text in lines.items():
            output_path = output_dir / f"{name}.ogg"
            if args.dry_run:
                print(f"{output_path.name}: {text}")
                continue
            if output_path.exists() and not args.overwrite:
                print(f"skip {output_path.name} (already exists)")
                continue

            print(f"generate {output_path.name}: {text}", flush=True)
            audio = synthesize(
                api_key=args.api_key,
                resource_id=args.resource_id,
                speaker=args.speaker,
                text=text,
                speech_rate=args.speech_rate,
                pitch=args.pitch,
                loudness_rate=args.loudness_rate,
                model=args.model,
                timeout=args.timeout,
            )
            output_dir.mkdir(parents=True, exist_ok=True)
            source_rate = save_as_16khz_ogg(audio, output_path)
            if source_rate == 16000:
                print(f"  saved {len(audio)} bytes, OpusHead=16000 Hz")
            else:
                print(
                    f"  service returned {source_rate} Hz; saved normalized 16 kHz Ogg Opus"
                )
            time.sleep(0.15)
    except TtsError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
