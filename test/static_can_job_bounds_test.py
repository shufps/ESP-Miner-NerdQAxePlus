from pathlib import Path


REPO = Path(__file__).resolve().parents[1]


def _read(relative_path: str) -> str:
    return (REPO / relative_path).read_text()


def _function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace_start = source.index("{", start)
    depth = 0
    for index in range(brace_start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace_start:index + 1]
    raise AssertionError(f"Could not find body for {signature}")


def test_asic_jobs_reject_out_of_range_job_ids_before_indexing() -> None:
    source = _read("main/tasks/asic_jobs.h")

    for signature in (
        "void storeJob(bm_job *next_job, uint8_t asic_job_id)",
        "bm_job *getClone(uint8_t asic_job_id)",
    ):
        body = _function_body(source, signature)
        guard_pos = body.index("asic_job_id >= MAX_ASIC_JOBS")
        index_pos = body.index("m_activeJobs[asic_job_id]")
        assert guard_pos < index_pos

    store_body = _function_body(source, "void storeJob(bm_job *next_job, uint8_t asic_job_id)")
    guard_start = store_body.index("asic_job_id >= MAX_ASIC_JOBS")
    guard_end = store_body.index("// if a slot was used before free it")
    guard_block = store_body[guard_start:guard_end]
    assert "free_bm_job(next_job);" in guard_block
    assert "return;" in guard_block


def test_can_master_drops_out_of_range_nonce_job_id_before_lookup() -> None:
    source = _read("main/tasks/can_master_task.cpp")
    body = _function_body(source, "static void handle_nonce")

    guard_pos = body.index("job_id >= MAX_ASIC_JOBS")
    lookup_pos = body.index("slaveAsicJobs[slave_id].getClone(job_id)")
    assert guard_pos < lookup_pos
    assert "out-of-range job_id" in body[guard_pos:lookup_pos]
    assert "return;" in body[guard_pos:lookup_pos]
