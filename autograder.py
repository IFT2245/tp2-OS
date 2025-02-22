# Created by Spokboud, adapted for 5 test modes: BASIC, NORMAL, MODES, EDGE, HIDDEN

import os
import re
import sys
import subprocess
from pathlib import Path
from typing import Tuple, List, Dict, Union
from dataclasses import dataclass
from enum import Enum

class AutograderError(Exception):
    pass

class NumberNotFoundException(Exception):
    def __init__(self, line):
        self.line = line
        self.message = f"The line '{self.line}' didn't contain an 8-digit number or any other number"
        super().__init__(self.message)

class FiveTestMode(str, Enum):
    BASIC = "BASIC"
    NORMAL = "NORMAL"
    MODES = "MODES"
    EDGE = "EDGE"
    HIDDEN = "HIDDEN"

@dataclass
class TestConfig:
    mode: FiveTestMode
    # We can store weighting or do simpler approach
    # We'll do a dictionary for aggregator indices
    aggregator_index_end: int
    # optional weighting or anything
    weights: Dict[str, float]
    additional_sources: List[str]
    valgrind_penalty_factor: float = 1.0

def get_test_config() -> TestConfig:
    mode_str = os.getenv('TEST_MODE', 'BASIC').upper()
    # parse as our FiveTestMode, fallback to BASIC
    try:
        mode = FiveTestMode(mode_str)
    except ValueError:
        print(f"Invalid test mode: {mode_str}, defaulting to BASIC")
        mode = FiveTestMode.BASIC

    # aggregator_index_end means how many of the 5 aggregator outputs we parse.
    # 0 => parse just r_basic
    # 1 => parse r_basic,r_normal
    # 2 => parse r_basic,r_normal,r_modes
    # 3 => parse r_basic,r_normal,r_modes,r_edge
    # 4 => parse all 5
    # We'll store simple weighting or none.
    mode_map = {
        FiveTestMode.BASIC:  TestConfig(mode, 0, {}, [], 1.0),
        FiveTestMode.NORMAL: TestConfig(mode, 1, {}, [], 1.0),
        FiveTestMode.MODES:  TestConfig(mode, 2, {}, [], 1.0),
        FiveTestMode.EDGE:   TestConfig(mode, 3, {}, ['test/edge-test.c'], 1.0),
        FiveTestMode.HIDDEN: TestConfig(mode, 4, {}, ['test/edge-test.c','test/hidden-test.c'], 1.0),
    }
    return mode_map[mode]

def extract_matricule(line: str, digits: int=None):
    pattern = r'\b(\d{8})\b' if digits else r'\b(\d*)\b'
    match = re.search(pattern, line)
    if not match:
        raise NumberNotFoundException(line)
    return match.group(1)

def find_team_info(file_path: Path) -> str:
    if not file_path.exists():
        return ""
    pairs = []
    lines = file_path.read_text().splitlines()
    for line in lines:
        line=line.strip()
        if not line:
            continue
        matricule = None
        for digits in [8, None]:
            try:
                matricule = extract_matricule(line, digits)
                break
            except NumberNotFoundException:
                pass
        if not matricule:
            continue
        name = line.replace(matricule, '').strip()
        pairs.append((matricule, name))
    result_parts = [f"{mat}-{name.replace(' ','-')}" for mat,name in pairs]
    return "_".join(result_parts)

def setup_directories() -> None:
    try:
        os.makedirs('test-results-full', exist_ok=True)
    except Exception as e:
        raise AutograderError(f"Failed to create directory: {e}")

def delete_file(filename: str) -> None:
    try:
        os.remove(filename)
    except OSError:
        pass


def cleanup_temp_files() -> None:
    temp_files = ['test_runner', 'valgrind.log']
    for temp_file in temp_files:
        if os.path.exists(temp_file):
            delete_file(temp_file)


def run_command(cmd: Union[str,List[str]], shell:bool=False)->Tuple[str,str]:
    try:
        result=subprocess.run(cmd,shell=shell,text=True,capture_output=True,check=True)
        return result.stdout,result.stderr
    except subprocess.CalledProcessError as e:
        return e.stdout or "", e.stderr or ""
    except Exception as e:
        raise AutograderError(f"Command execution failed: {e}")

def create_test_wrapper(cfg: TestConfig) -> None:
    # We'll produce a code that calls all 5 aggregator references in order:
    #   int r_basic= run_basic_tests();
    #   int r_normal= run_normal_tests();
    #   int r_modes= run_modes_tests();
    #   int r_edge= run_edge_tests();
    #   int r_hidden= run_hidden_tests();
    #   printf("%d %d %d %d %d\n", r_basic, r_normal, r_modes, r_edge, r_hidden);
    includes = [
        '#include <stdio.h>',
        '#include "../test/basic-test.h"',
        '#include "../test/normal-test.h"',
        '#include "../test/modes-test.h"',
        '#include "../test/edge-test.h"',
        '#include "../test/hidden-test.h"'
    ]
    aggregator=[
        'int r_basic=run_basic_tests();',
        'int r_normal=run_normal_tests();',
        'int r_modes=run_modes_tests();',
        'int r_edge=run_edge_tests();',
        'int r_hidden=run_hidden_tests();'
    ]
    wrapper_code = f"""
{chr(10).join(includes)}

int main(void){{
    {chr(10).join(aggregator)}
    // We'll just print all 5 aggregator results in one line:
    printf("%d %d %d %d %d\\n",r_basic,r_normal,r_modes,r_edge,r_hidden);
    return 0;
}}
"""
    try:
        Path('src/test_wrapper.c').write_text(wrapper_code)
    except Exception as e:
        raise AutograderError(f"Failed to create test_wrapper: {e}")

def run_tests(cfg: TestConfig)->Tuple[str,str]:
    try:
        setup_directories()
        create_test_wrapper(cfg)
        compile_cmd=[
            'gcc','-o','test_runner',
            'src/test_wrapper.c',
            # Our aggregator test c's:
            'test/basic-test.c',
            'test/normal-test.c',
            'test/modes-test.c',
            'test/edge-test.c',
            'test/hidden-test.c',
            # Possibly your product sources:
            'src/os.c','src/ready.c','src/worker.c','src/tokenizer.c','src/parser.c','src/shell.c','src/scheduler.c',
            '-I./test','-I./src','-I./include','-lpthread'
        ]
        compile_cmd.extend(cfg.additional_sources)

        # run compile
        stdout,stderr=run_command(compile_cmd)
        if stderr:
            raise AutograderError(f"Compilation failed: {stderr}")

        # run test_runner
        test_output,_=run_command(['./test_runner'])

        # valgrind
        valgrind_cmd=[
            'valgrind','--leak-check=full','--show-leak-kinds=all','--track-origins=yes','--log-file=valgrind.log','./test_runner'
        ]
        run_command(valgrind_cmd)

        with open('valgrind.log','r') as f:
            valg_out=f.read()
        return test_output,valg_out
    except Exception as e:
        raise AutograderError(f"Test execution failed: {e}")
    finally:
        cleanup_temp_files()

def parse_5integers(line:str)->List[int]:
    parts=line.strip().split()
    if len(parts)!=5:
        return []
    if all(x.isdigit() for x in parts):
        return [int(x) for x in parts]
    return []

def parse_scores(test_output:str, cfg:TestConfig)->Tuple[List[int],float]:
    # We'll parse lines for "X X X X X". Then we only care up to aggregator_index_end +1
    # aggregator_index_end in [0..4].
    # We'll produce a sum or some simpler approach. We'll do a naive approach: sum them or average them?
    # Example: if aggregator_index_end=2 => we parse [r_basic,r_normal,r_modes].
    # We'll do an average of those or just sum for the "score"? Up to you.
    lines=test_output.splitlines()
    r=[0,0,0,0,0]
    for ln in lines:
        ints=parse_5integers(ln)
        if len(ints)==5:
            # we store them in r
            r=ints
            break
    # aggregator_index_end => parse up to that index
    # e.g. if aggregator_index_end=0 => we only check r[0]
    # aggregator_index_end=2 => we check r[0],r[1],r[2].
    # We'll do a naive approach: sum of those aggregator passing => produce percentage = (#passed / count)*100
    # aggregator_value=0 => fail,  aggregator_value=0 => means test aggregator had error, 1 => pass?
    # We assume aggregator returns 0 => PASS, 1 => FAIL
    # We'll interpret "0 => pass" as 1, "1 => fail" as 0.
    # Then we do (#passed / count)*100
    count= cfg.aggregator_index_end+1
    if count<=0 or count>5:
        count=5
    pass_sum=0
    for i in range(count):
        pass_sum += (1 if r[i]==0 else 0)
    success_rate= (pass_sum*100.0)/count
    return r,success_rate

def analyze_valgrind(output:str,cfg:TestConfig)->Tuple[float,List[str],List[str]]:
    leaks=[]
    invalids=[]
    # memory leaks
    leak_matches=re.findall(r'(definitely|indirectly) lost: [1-9].*',output)
    leaks.extend(leak_matches)
    leak_penalty=10 if leaks else 0

    invalid_reads=re.findall(r'Invalid read of size \\d+',output)
    invalid_writes=re.findall(r'Invalid write of size \\d+',output)
    invalids.extend(invalid_reads)
    invalids.extend(invalid_writes)
    invalid_penalty=min(len(invalids),5)

    total_penalty=-(leak_penalty+invalid_penalty)*cfg.valgrind_penalty_factor
    return total_penalty,leaks,invalids

def handle_results_file(matricule:str,score:float,test_out:str,valgrind_out:str,leaks:List[str],invalids:List[str],raw_score:float,total_penalty:float,cfg:TestConfig)->None:
    try:
        Path("test-results-full").mkdir(exist_ok=True)
        out_file=Path("test-results-full") / f"{matricule}.txt"
        with out_file.open("w") as f:
            f.write(f"Test Mode: {cfg.mode}\n\n")
            f.write(f"Test Output:\n{test_out}\n\n")
            f.write(f"Raw Score(based on aggregator partial pass): {raw_score:.1f}\n")
            f.write(f"Memory leaks found: {len(leaks)}\n")
            for l in leaks:
                f.write(f" - {l}\n")
            f.write(f"Invalid ops found: {len(invalids)}\n")
            for i in invalids:
                f.write(f" - {i}\n")
            f.write(f"Penalty: {total_penalty:.2f}\n")
            f.write(f"Final Score: {score:.1f}\n\n")
            f.write("Valgrind Output:\n")
            f.write(valgrind_out)
        print(f"RAW:{{{raw_score}}}")
        print(f"GRADE:{{{score}}}")
        print(f"PENALTY:{{{total_penalty}}}")
        print(f"LEAKS:{{{len(leaks)}}}")
        print(f"INVALIDS:{{{len(invalids)}}}")
        print(f"MODE:{{{cfg.mode}}}")
        print(f"ID:{{{matricule}}}")
        print(f"Results saved to: {out_file}")
    except Exception as e:
        raise AutograderError(f"Failed to handle results file: {e}")

def main()->int:
    try:
        cfg=get_test_config()
        # gather names
        team_str=""
        if Path("names.txt").exists():
            team_str=find_team_info(Path("names.txt"))
        if not team_str:
            raise AutograderError("No names.txt or no valid matricules found.")

        test_out,valg_out=run_tests(cfg)

        aggregator_vals,calc_rate=parse_scores(test_out,cfg)
        if calc_rate<0:
            raise AutograderError(f"Could not parse aggregator lines from test output:\n{test_out}")

        valg_penalty,leaks,invalids=analyze_valgrind(valg_out,cfg)
        final_score=calc_rate+valg_penalty if calc_rate>=abs(valg_penalty) else calc_rate

        # e.g. "TEAMSTRING$finalscore$"
        result_name=f"{team_str}${final_score:.1f}$"

        handle_results_file(
            matricule=result_name,
            score=final_score,
            test_out=test_out,
            valgrind_out=valg_out,
            leaks=leaks,
            invalids=invalids,
            raw_score=calc_rate,
            total_penalty=valg_penalty,
            cfg=cfg
        )
        print(f"GRADE:{{{final_score}}}")
        return 0
    except Exception as e:
        print(f"Fatal error: {e}")
        return 1
    finally:
        cleanup_temp_files()

if __name__=="__main__":
    sys.exit(main())
