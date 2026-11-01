import json
from pathlib import Path
import traceback
import ida_auto
import ida_ida
import ida_loader
import ida_nalt
import ida_pro
import idc

build_dir = Path(__file__).resolve().parent.parent / "build"
database_dir = build_dir / "ida-probe"
database_dir.mkdir(parents=True, exist_ok=True)
output = build_dir / "ida-load-probe.json"
try:
    ida_auto.enable_auto(False)
    result = {
        "input": ida_nalt.get_input_file_path(),
        "root": ida_nalt.get_root_filename(),
        "min_ea": hex(ida_ida.inf_get_min_ea()),
        "max_ea": hex(ida_ida.inf_get_max_ea()),
    }
    result["saved"] = bool(idc.save_database(str(database_dir / "DivaMegaMix.i64")))
    with open(output, "w", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    ida_pro.qexit(0)
except Exception:
    with open(output, "w", encoding="utf-8") as stream:
        stream.write(traceback.format_exc())
    ida_pro.qexit(1)
