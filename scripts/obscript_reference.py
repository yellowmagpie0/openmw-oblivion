#!/usr/bin/env python3
"""Independent ObScript parser and native-IR emitter used by the M6 audit.

This intentionally does not import or invoke the C++ frontend.  It consumes an
esmtool ``obscript`` JSON report, reconstructs the AST and Program using a
separate Python implementation, and verifies both canonical fingerprints.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


KEYWORDS = {
    "scn", "scriptname", "begin", "end", "if", "elseif", "else", "endif",
    "set", "to", "return", "short", "int", "long", "float", "ref",
}
DECLARATIONS = {"short", "int", "long", "float", "ref"}
TOKEN_RE = re.compile(
    r"(?P<COMMENT>;[^\r\n]*)|"
    r"(?P<NEWLINE>\r\n|\r|\n)|"
    r"(?P<WS>[ \t]+)|"
    r"(?P<FLOAT>\d+\.\d*(?![A-Za-z_])|\.\d+(?![A-Za-z_]))|"
    r"(?P<NAME_NUM>\d+[A-Za-z_][A-Za-z0-9_]*)|"
    r"(?P<INT>\d+)|"
    r'(?P<STRING>"[^"\r\n]*")|'
    r"(?P<OP>&&|\|\||==|!=|<=|>=|:=|<|>|\+|-|\*|/|%|\(|\)|,|\.|=)|"
    r"(?P<JUNK>[`'!:?@#$^&|\[\]{}~\\]+)|"
    r"(?P<NAME>[A-Za-z_][A-Za-z0-9_]*)"
)


@dataclass(frozen=True)
class Token:
    kind: str
    value: str


def tokenize(source: str) -> list[Token]:
    result: list[Token] = []
    offset = 0
    while offset < len(source):
        match = TOKEN_RE.match(source, offset)
        if match is None:
            raise SyntaxError(f"unexpected character at offset {offset}")
        kind = match.lastgroup
        assert kind is not None
        value = match.group()
        offset = match.end()
        if kind == "NEWLINE":
            result.append(Token("NEWLINE", "\n"))
        elif kind in {"WS", "COMMENT", "JUNK"}:
            continue
        elif kind == "NAME_NUM":
            result.append(Token("NAME", value))
        elif kind == "NAME" and value.lower() in KEYWORDS:
            result.append(Token("KEYWORD", value.lower()))
        elif kind == "STRING":
            result.append(Token(kind, value[1:-1]))
        else:
            result.append(Token(kind, value))
    result.extend((Token("NEWLINE", "\n"), Token("EOF", "")))
    return result


def node(kind: str, **values: Any) -> dict[str, Any]:
    return {"kind": kind, **values}


class Parser:
    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.index = 0

    def peek(self, offset: int = 0) -> Token:
        return self.tokens[min(self.index + offset, len(self.tokens) - 1)]

    def take(self) -> Token:
        value = self.peek()
        if value.kind != "EOF":
            self.index += 1
        return value

    def accept(self, kind: str, value: str | None = None) -> Token | None:
        current = self.peek()
        if current.kind != kind or (value is not None and current.value.lower() != value):
            return None
        return self.take()

    def expect(self, kind: str, value: str | None = None) -> Token:
        accepted = self.accept(kind, value)
        if accepted is None:
            raise SyntaxError(f"expected {value or kind}, got {self.peek()!r}")
        return accepted

    def skip_newlines(self) -> None:
        while self.accept("NEWLINE"):
            pass

    def skip_rest(self) -> None:
        while self.peek().kind not in {"NEWLINE", "EOF"}:
            self.take()

    def end_line(self) -> None:
        if self.peek().kind != "EOF":
            self.expect("NEWLINE")
            self.skip_newlines()

    def keyword(self, value: str) -> bool:
        return self.peek().kind == "KEYWORD" and self.peek().value == value

    def parse(self) -> dict[str, Any]:
        self.skip_newlines()
        name = None
        if self.keyword("scn") or self.keyword("scriptname"):
            self.take()
            name = self.expect("NAME").value
            self.skip_rest()
            self.end_line()
        variables: list[dict[str, Any]] = []
        blocks: list[dict[str, Any]] = []
        stray: list[dict[str, Any]] = []
        while self.peek().kind != "EOF":
            self.skip_newlines()
            if self.peek().kind == "EOF":
                break
            if self.peek().kind == "KEYWORD" and self.peek().value in DECLARATIONS:
                type_name = self.take().value
                variables.append(node("Var", type=type_name, name=self.expect("NAME").value))
                self.skip_rest()
                self.end_line()
            elif self.keyword("begin"):
                blocks.append(self.parse_block())
            else:
                stray.append(self.parse_statement())
        return node("Script", name=name, variables=variables, blocks=blocks, stray=stray)

    def parse_block(self) -> dict[str, Any]:
        self.expect("KEYWORD", "begin")
        event = self.take()
        if event.kind not in {"NAME", "KEYWORD"}:
            raise SyntaxError("expected block event")
        arguments: list[dict[str, Any]] = []
        while self.peek().kind not in {"NEWLINE", "EOF"}:
            if self.accept("OP", ",") is None:
                arguments.append(self.parse_expression())
        self.end_line()
        body = self.parse_statements({"end"})
        self.expect("KEYWORD", "end")
        self.skip_rest()
        self.end_line()
        return node("Block", event=event.value, arguments=arguments, body=body)

    def parse_statements(self, until: set[str]) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        while True:
            self.skip_newlines()
            if self.peek().kind == "EOF" or (
                self.peek().kind == "KEYWORD" and self.peek().value in until
            ):
                return result
            result.append(self.parse_statement())

    def parse_statement(self) -> dict[str, Any]:
        first = self.peek()
        if first.kind == "KEYWORD":
            if first.value in {"endif", "elseif", "else"}:
                self.take()
                self.skip_rest()
                self.end_line()
                return node("Stray", keyword=first.value)
            if first.value == "set":
                return self.parse_set()
            if first.value == "if":
                return self.parse_if()
            if first.value == "return":
                self.take()
                self.skip_rest()
                self.end_line()
                return node("Return")
            if first.value in DECLARATIONS:
                self.take()
                result = node("Var", type=first.value, name=self.expect("NAME").value)
                self.skip_rest()
                self.end_line()
                return result
        if first.kind == "OP" and first.value != "(":
            self.skip_rest()
            self.end_line()
            return node("Junk")
        if (
            first.kind == "NAME" and self.peek(1) == Token("OP", ".")
            and self.peek(2) == Token("KEYWORD", "set")
        ):
            base = node("Name", value=self.take().value)
            self.take()
            self.take()
            target = node("Member", base=base, member=self.parse_postfix())
            self.expect("KEYWORD", "to")
            value = self.parse_expression()
            self.end_line()
            return node("Set", target=target, value=value)
        expression = self.parse_command_line()
        self.end_line()
        return node("Expression", value=expression)

    def parse_set(self) -> dict[str, Any]:
        self.expect("KEYWORD", "set")
        target = self.parse_postfix()
        self.expect("KEYWORD", "to")
        value = self.parse_expression()
        self.end_line()
        return node("Set", target=target, value=value)

    def parse_if(self) -> dict[str, Any]:
        self.expect("KEYWORD", "if")
        condition = self.parse_expression()
        self.end_line()
        clauses = [node("Clause", condition=condition,
                        body=self.parse_statements({"elseif", "else", "endif", "end"}))]
        while True:
            if self.keyword("elseif"):
                self.take()
                condition = self.parse_expression()
                self.end_line()
                clauses.append(node("Clause", condition=condition,
                                    body=self.parse_statements({"elseif", "else", "endif", "end"})))
            elif self.keyword("else"):
                self.take()
                if self.keyword("if"):
                    self.take()
                    condition = self.parse_expression()
                    self.end_line()
                    clauses.append(node("Clause", condition=condition,
                                        body=self.parse_statements({"elseif", "else", "endif", "end"})))
                else:
                    self.skip_rest()
                    self.end_line()
                    clauses.append(node("Clause", condition=None,
                                        body=self.parse_statements({"endif", "end"})))
            else:
                break
        if self.keyword("endif"):
            self.take()
            self.skip_rest()
            self.end_line()
        return node("If", clauses=clauses)

    def parse_command_line(self) -> dict[str, Any]:
        result = self.parse_expression()
        extra: list[dict[str, Any]] = []
        while self.peek().kind not in {"NEWLINE", "EOF"}:
            if self.accept("OP", ",") is None:
                extra.append(self.parse_expression())
        if extra:
            if result["kind"] == "Call":
                result["arguments"].extend(extra)
            else:
                result = node("Call", callee=result, arguments=extra)
        return result

    def parse_expression(self) -> dict[str, Any]:
        return self.parse_binary(self.parse_and, {"||"})

    def parse_and(self) -> dict[str, Any]:
        return self.parse_binary(self.parse_comparison, {"&&"})

    def parse_comparison(self) -> dict[str, Any]:
        operators = {"==", "!=", "<", ">", "<=", ">="}
        if self.peek().kind == "OP" and self.peek().value in operators:
            result = node("Missing")
        else:
            result = self.parse_addition()
        while self.peek().kind == "OP" and self.peek().value in operators:
            operator = self.take().value
            result = node("Binary", operator=operator, left=result, right=self.parse_addition())
        return result

    def parse_addition(self) -> dict[str, Any]:
        return self.parse_binary(self.parse_multiplication, {"+", "-"})

    def parse_multiplication(self) -> dict[str, Any]:
        return self.parse_binary(self.parse_unary, {"*", "/", "%"})

    def parse_binary(self, operand: Any, operators: set[str]) -> dict[str, Any]:
        result = operand()
        while self.peek().kind == "OP" and self.peek().value in operators:
            operator = self.take().value
            result = node("Binary", operator=operator, left=result, right=operand())
        return result

    def parse_unary(self) -> dict[str, Any]:
        if self.accept("OP", "-"):
            return node("Negate", value=self.parse_unary())
        return self.parse_call()

    def parse_call(self) -> dict[str, Any]:
        result = self.parse_postfix()
        arguments: list[dict[str, Any]] = []
        while True:
            if self.peek() == Token("OP", ",") and arguments:
                self.take()
                continue
            if self.peek().kind in {"NAME", "INT", "FLOAT", "STRING"}:
                arguments.append(self.parse_postfix())
                continue
            break
        return node("Call", callee=result, arguments=arguments) if arguments else result

    def parse_postfix(self) -> dict[str, Any]:
        result = self.parse_primary()
        while self.accept("OP", "."):
            if self.peek().kind == "KEYWORD":
                member = node("Name", value=self.take().value)
            else:
                member = self.parse_primary()
            result = node("Member", base=result, member=member)
        return result

    def parse_primary(self) -> dict[str, Any]:
        value = self.peek()
        if value.kind == "INT":
            self.take()
            return node("Integer", value=int(value.value))
        if value.kind == "FLOAT":
            self.take()
            return node("Float", value=float(value.value))
        if value.kind == "STRING":
            self.take()
            return node("String", value=value.value)
        if self.accept("OP", "("):
            result = self.parse_expression()
            arguments: list[dict[str, Any]] = []
            while self.peek().kind not in {"NEWLINE", "EOF"} and self.peek() != Token("OP", ")"):
                if self.accept("OP", ",") is None:
                    arguments.append(self.parse_expression())
            self.expect("OP", ")")
            return node("Call", callee=result, arguments=arguments) if arguments else result
        if value.kind == "NAME" or value == Token("KEYWORD", "to"):
            self.take()
            return node("Name", value=value.value)
        raise SyntaxError(f"unexpected expression token {value!r}")


def atom(value: str) -> str:
    return f"{len(value.encode('utf-8'))}:{value}"


def float_bits(value: float) -> str:
    return f"{struct.unpack('>Q', struct.pack('>d', value))[0]:016x}"


def canonical_expression(value: dict[str, Any]) -> str:
    kind = value["kind"]
    if kind == "Missing":
        return "(M)"
    if kind == "Integer":
        return f"(I{value['value']})"
    if kind == "Float":
        return f"(F{float_bits(value['value'])})"
    if kind == "String":
        return f"(S{atom(value['value'])})"
    if kind == "Name":
        return f"(N{atom(value['value'])})"
    if kind == "Negate":
        return f"(-{canonical_expression(value['value'])})"
    if kind == "Binary":
        return (f"(B{atom(value['operator'])}{canonical_expression(value['left'])}"
                f"{canonical_expression(value['right'])})")
    if kind == "Member":
        return f"(D{canonical_expression(value['base'])}{canonical_expression(value['member'])})"
    if kind == "Call":
        return ("(C" + canonical_expression(value["callee"])
                + "".join(canonical_expression(item) for item in value["arguments"]) + ")")
    raise AssertionError(kind)


def canonical_statements(values: list[dict[str, Any]]) -> str:
    return "[" + "".join(canonical_statement(value) for value in values) + "]"


def canonical_statement(value: dict[str, Any]) -> str:
    kind = value["kind"]
    if kind == "Var":
        return f"(V{atom(value['type'])}{atom(value['name'])})"
    if kind == "Set":
        return f"(S{canonical_expression(value['target'])}{canonical_expression(value['value'])})"
    if kind == "If":
        clauses = ""
        for clause in value["clauses"]:
            condition = clause["condition"]
            clauses += ("(K" + ("1" + canonical_expression(condition) if condition is not None else "0")
                        + canonical_statements(clause["body"]) + ")")
        return f"(I{clauses})"
    if kind == "Return":
        return "(R)"
    if kind == "Expression":
        return f"(E{canonical_expression(value['value'])})"
    if kind == "Stray":
        return f"(K{atom(value['keyword'])})"
    if kind == "Junk":
        return "(J)"
    raise AssertionError(kind)


def canonical_script(value: dict[str, Any]) -> str:
    name = value["name"]
    result = "(S" + ("1" + atom(name) if name is not None else "0") + "["
    result += "".join(f"(V{atom(item['type'])}{atom(item['name'])})" for item in value["variables"])
    result += "]["
    for block in value["blocks"]:
        result += f"(B{atom(block['event'])}["
        result += "".join(canonical_expression(argument) for argument in block["arguments"])
        result += "]" + canonical_statements(block["body"]) + ")"
    return result + "]" + canonical_statements(value["stray"]) + ")"


TYPE_MAP = {"short": "short", "int": "int", "long": "long", "float": "float", "ref": "ref"}
FUNCTION_PREFIXES = ("get", "is", "has", "can", "which", "exists", "menumode")


def instruction(opcode: str, type_name: str, *, text: str = "", integer: int = 0,
                floating: float = 0.0, index: int = 0, arguments: int = 0,
                member: bool = False) -> dict[str, Any]:
    return {"opcode": opcode, "type": type_name, "text": text, "integer": integer,
            "float": floating, "index": index, "arguments": arguments, "member": member}


class Emitter:
    def __init__(self, unit_id: str, script: dict[str, Any], references: list[str] | None = None):
        self.unit_id = unit_id
        self.script = script
        self.references = references or []
        self.locals: list[dict[str, str]] = []

    def emit(self) -> dict[str, Any]:
        for declaration in self.script["variables"]:
            self.add_local(declaration)
        for block in self.script["blocks"]:
            self.collect_locals(block["body"])
        self.collect_locals(self.script["stray"])
        entries = [self.emit_entry(block["event"], block["arguments"], block["body"])
                   for block in self.script["blocks"]]
        if self.script["stray"]:
            entries.append(self.emit_entry("__stray", [], self.script["stray"]))
        return {"unit": self.unit_id, "name": self.script["name"],
                "locals": self.locals, "references": self.references, "entries": entries}

    def add_local(self, declaration: dict[str, Any]) -> None:
        self.locals.append({"name": declaration["name"], "type": declaration["type"]})

    def collect_locals(self, statements: list[dict[str, Any]]) -> None:
        for statement in statements:
            if statement["kind"] == "Var":
                self.add_local(statement)
            for clause in statement.get("clauses", []):
                self.collect_locals(clause["body"])

    def find_local(self, name: str) -> int | None:
        lowered = name.lower()
        return next((index for index, item in enumerate(self.locals)
                     if item["name"].lower() == lowered), None)

    def emit_entry(self, event: str, arguments: list[dict[str, Any]],
                   statements: list[dict[str, Any]]) -> dict[str, Any]:
        code: list[dict[str, Any]] = []
        self.emit_statements(statements, code)
        return {"event": event, "arguments": [canonical_expression(item) for item in arguments], "code": code}

    @staticmethod
    def function_like(name: str) -> bool:
        return name.lower().startswith(FUNCTION_PREFIXES)

    @staticmethod
    def member_name(value: dict[str, Any]) -> str:
        if value["kind"] in {"Name", "String"}:
            return value["value"]
        if value["kind"] == "Integer":
            return str(value["value"])
        if value["kind"] == "Member":
            return Emitter.member_name(value["member"])
        return canonical_expression(value)

    def emit_expression(self, value: dict[str, Any], code: list[dict[str, Any]]) -> str:
        kind = value["kind"]
        if kind == "Missing":
            code.append(instruction("push-missing", "int"))
            return "int"
        if kind == "Integer":
            code.append(instruction("push-int", "long", integer=value["value"]))
            return "long"
        if kind == "Float":
            code.append(instruction("push-float", "float", floating=value["value"]))
            return "float"
        if kind == "String":
            code.append(instruction("push-string", "string", text=value["value"]))
            return "string"
        if kind == "Name":
            local = self.find_local(value["value"])
            if local is not None:
                type_name = TYPE_MAP[self.locals[local]["type"]]
                code.append(instruction("load-local", type_name, index=local))
                return type_name
            if self.function_like(value["value"]):
                code.append(instruction("call", "unknown", text=value["value"].lower()))
                return "unknown"
            code.append(instruction("load-ref", "ref", text=value["value"].lower()))
            return "ref"
        if kind == "Negate":
            type_name = self.emit_expression(value["value"], code)
            code.append(instruction("negate", type_name))
            return type_name
        if kind == "Binary":
            left = self.emit_expression(value["left"], code)
            right = self.emit_expression(value["right"], code)
            if value["operator"] in {"==", "!=", "<", ">", "<=", ">=", "&&", "||"}:
                type_name = "bool"
            else:
                type_name = "float" if "float" in {left, right} else "long"
            code.append(instruction("binary", type_name, text=value["operator"]))
            return type_name
        if kind == "Member":
            self.emit_expression(value["base"], code)
            name = self.member_name(value["member"])
            if self.function_like(name):
                code.append(instruction("call", "unknown", text=name.lower(), member=True))
            else:
                code.append(instruction("load-member", "unknown", text=name.lower()))
            return "unknown"
        if kind == "Call":
            callee = value["callee"]
            member = False
            if callee["kind"] == "Member":
                member = True
                self.emit_expression(callee["base"], code)
                name = self.member_name(callee["member"])
            elif callee["kind"] == "Name":
                name = callee["value"]
            else:
                self.emit_expression(callee, code)
                name = "__expression"
            for argument in value["arguments"]:
                self.emit_expression(argument, code)
            code.append(instruction("call", "unknown", text=name.lower(),
                                    arguments=len(value["arguments"]), member=member))
            return "unknown"
        raise AssertionError(kind)

    def emit_statements(self, statements: list[dict[str, Any]], code: list[dict[str, Any]]) -> None:
        for statement in statements:
            kind = statement["kind"]
            if kind in {"Var", "Stray", "Junk"}:
                continue
            if kind == "Return":
                code.append(instruction("return", "void"))
            elif kind == "Expression":
                expression = statement["value"]
                if expression["kind"] == "Name" and self.find_local(expression["value"]) is None:
                    code.append(instruction("call", "unknown", text=expression["value"].lower()))
                elif expression["kind"] == "Member":
                    self.emit_expression(expression["base"], code)
                    code.append(instruction("call", "unknown",
                                            text=self.member_name(expression["member"]).lower(), member=True))
                else:
                    self.emit_expression(expression, code)
                    code.append(instruction("discard", "void"))
            elif kind == "Set":
                target = statement["target"]
                if target["kind"] == "Member":
                    self.emit_expression(target["base"], code)
                source_type = self.emit_expression(statement["value"], code)
                if target["kind"] == "Name":
                    local = self.find_local(target["value"])
                    if local is None:
                        code.append(instruction("store-external", source_type, text=target["value"].lower()))
                    else:
                        destination = TYPE_MAP[self.locals[local]["type"]]
                        if source_type != destination and destination != "ref":
                            code.append(instruction("convert", destination))
                        code.append(instruction("store-local", destination, index=local))
                elif target["kind"] == "Member":
                    code.append(instruction("store-member", source_type,
                                            text=self.member_name(target["member"]).lower()))
                else:
                    raise ValueError("invalid set target")
            elif kind == "If":
                end_jumps: list[int] = []
                for index, clause in enumerate(statement["clauses"]):
                    false_jump = None
                    if clause["condition"] is not None:
                        self.emit_expression(clause["condition"], code)
                        false_jump = len(code)
                        code.append(instruction("jump-if-false", "void"))
                    self.emit_statements(clause["body"], code)
                    if index + 1 < len(statement["clauses"]):
                        end_jumps.append(len(code))
                        code.append(instruction("jump", "void"))
                    if false_jump is not None:
                        code[false_jump]["index"] = len(code)
                for jump in end_jumps:
                    code[jump]["index"] = len(code)
            else:
                raise AssertionError(kind)


def canonical_instruction(value: dict[str, Any]) -> str:
    return (f"(I{atom(value['opcode'])}{atom(value['type'])}{atom(value['text'])}"
            f"{value['integer']}:{float_bits(value['float'])}:{value['index']}:"
            f"{value['arguments']}:{'1' if value['member'] else '0'})")


def canonical_program(value: dict[str, Any]) -> str:
    result = "(P" + atom(value["unit"])
    result += "1" + atom(value["name"].lower()) if value["name"] is not None else "0"
    result += "["
    for local in value["locals"]:
        result += f"(L{atom(local['name'].lower())}{atom(local['type'])})"
    result += "][" + "".join(atom(reference) for reference in value["references"]) + "]["
    for entry in value["entries"]:
        result += f"(E{atom(entry['event'].lower())}["
        result += "".join(atom(argument) for argument in entry["arguments"])
        result += "][" + "".join(canonical_instruction(item) for item in entry["code"]) + "])"
    return result + "])"


def fingerprint(value: str) -> str:
    result = 14695981039346656037
    for byte in value.encode("utf-8"):
        result ^= byte
        result = (result * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return f"fnv1a64:{result:016x}"


def verify_report(report: dict[str, Any]) -> dict[str, Any]:
    failures: list[dict[str, str]] = []
    checked = 0
    for unit in report["units"]:
        if unit["source"] is None:
            failures.append({"id": unit["id"], "kind": "missing-source"})
            continue
        try:
            script = Parser(tokenize(unit["source"])).parse()
            ast_fingerprint = fingerprint(canonical_script(script))
            program = Emitter(unit["id"], script, unit.get("references", [])).emit()
            program_fingerprint = fingerprint(canonical_program(program))
            checked += 1
            if ast_fingerprint != unit["ast_fingerprint"]:
                failures.append({"id": unit["id"], "kind": "ast",
                                 "expected": unit["ast_fingerprint"], "actual": ast_fingerprint})
            if program_fingerprint != unit["program_fingerprint"]:
                failures.append({"id": unit["id"], "kind": "program",
                                 "expected": unit["program_fingerprint"], "actual": program_fingerprint})
        except Exception as error:  # reported with stable unit identity
            failures.append({"id": unit["id"], "kind": "exception", "message": str(error)})
    return {"schema_version": 1, "checked_units": checked, "failure_count": len(failures),
            "failures": failures}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    with arguments.report.open(encoding="utf-8") as stream:
        result = verify_report(json.load(stream))
    serialized = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if arguments.output:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(serialized, encoding="utf-8")
    else:
        sys.stdout.write(serialized)
    return 0 if result["failure_count"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
