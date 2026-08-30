'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');

const BRACKETED_PASTE_START = '\x1b[200~';
const BRACKETED_PASTE_END = '\x1b[201~';
const BRACKETED_PASTE_ENABLE = '\x1b[?2004h';
const BRACKETED_PASTE_DISABLE = '\x1b[?2004l';
const CTRL_C = '\x03';
const BACKSPACE = '\x7f';
const ALT_BACKSPACE = '\b';
const PROMPT = '❯ ';

function formatLocalTimestamp(date = new Date()) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hour = String(date.getHours()).padStart(2, '0');
    const minute = String(date.getMinutes()).padStart(2, '0');
    const second = String(date.getSeconds()).padStart(2, '0');
    const millisecond = String(date.getMilliseconds()).padStart(3, '0');
    const offsetMinutes = -date.getTimezoneOffset();
    const sign = offsetMinutes >= 0 ? '+' : '-';
    const absoluteOffsetMinutes = Math.abs(offsetMinutes);
    const offsetHours = String(Math.floor(absoluteOffsetMinutes / 60)).padStart(2, '0');
    const offsetRemainderMinutes = String(absoluteOffsetMinutes % 60).padStart(2, '0');

    return `${year}-${month}-${day}T${hour}:${minute}:${second}.${millisecond}${sign}${offsetHours}:${offsetRemainderMinutes}`;
}

function resolveLogsDirectory() {
    const home = process.env.USERPROFILE || os.homedir();
    return path.join(home, '.wt', 'logs');
}

function defaultLogPath() {
    return path.join(resolveLogsDirectory(), 'copilot-terminal-simulator.jsonl');
}

function escapeForLog(text) {
    return text
        .replace(/\r/g, '\\r')
        .replace(/\n/g, '\\n')
        .replace(/\t/g, '\\t')
        .replace(/\x1b/g, '\\u001b');
}

function bufferToHex(buffer) {
    return buffer.toString('hex');
}

function trailingControlPrefixLength(text) {
    const sequences = [BRACKETED_PASTE_START, BRACKETED_PASTE_END];
    const maxLength = Math.max(...sequences.map((value) => value.length)) - 1;

    for (let length = Math.min(maxLength, text.length); length > 0; length -= 1) {
        const suffix = text.slice(-length);
        if (sequences.some((sequence) => sequence.startsWith(suffix))) {
            return length;
        }
    }

    return 0;
}

function consumeEscapeSequence(text, startIndex, limit) {
    if (text[startIndex] !== '\x1b') {
        return 0;
    }

    if (text.startsWith(BRACKETED_PASTE_START, startIndex)) {
        return BRACKETED_PASTE_START.length;
    }

    if (text.startsWith(BRACKETED_PASTE_END, startIndex)) {
        return BRACKETED_PASTE_END.length;
    }

    if (startIndex + 1 >= limit) {
        return 0;
    }

    if (text[startIndex + 1] !== '[') {
        return 1;
    }

    for (let index = startIndex + 2; index < limit; index += 1) {
        const ch = text[index];
        if ((ch >= '@' && ch <= '~') || ch === '~') {
            return index - startIndex + 1;
        }
    }

    return 0;
}

function mergeOrigin(currentOrigin, nextOrigin) {
    if (!currentOrigin) {
        return nextOrigin;
    }

    if (currentOrigin === nextOrigin) {
        return currentOrigin;
    }

    return 'mixed';
}

function createJsonlLogger(logPath = defaultLogPath()) {
    fs.mkdirSync(path.dirname(logPath), { recursive: true });

    return {
        logPath,
        append(event, payload = {}) {
            const record = {
                ts: formatLocalTimestamp(),
                event,
                pid: process.pid,
                payload,
            };
            fs.appendFileSync(logPath, `${JSON.stringify(record)}\n`, 'utf8');
        },
    };
}

class CopilotTerminalSimulator {
    constructor(options = {}) {
        this._write = options.write || ((text) => process.stdout.write(text));
        this._schedule = options.schedule || ((callback, delay) => setTimeout(callback, delay));
        this._onExit = options.onExit || (() => {});
        this._log = options.log || (() => {});
        this._restoreTerminal = options.restoreTerminal || (() => {});

        this._currentInput = '';
        this._currentOrigin = '';
        this._pendingControlText = '';
        this._inBracketedPaste = false;
        this._closed = false;
    }

    start() {
        this._writeOutput(BRACKETED_PASTE_ENABLE, 'output.mode');
        this._log('session.start', {
            bracketedPasteEnabled: true,
        });

        this._writeOutput('Copilot terminal simulator\r\n');
        this._writeOutput('Bracketed paste is ON. Press Enter to submit, Ctrl+C to exit.\r\n\r\n');
        this._renderPrompt();
    }

    handleBuffer(buffer) {
        if (this._closed || !buffer || buffer.length === 0) {
            return;
        }

        const chunk = buffer.toString('utf8');
        this._log('input.raw', {
            hex: bufferToHex(buffer),
            utf8: escapeForLog(chunk),
            byteLength: buffer.length,
        });

        if (chunk.includes(CTRL_C)) {
            this.shutdown('ctrl-c');
            return;
        }

        this._pendingControlText += chunk;
        this._drainPending(false);
    }

    shutdown(reason) {
        if (this._closed) {
            return;
        }

        this._closed = true;
        this._drainPending(true);
        this._log('session.stop', {
            reason,
        });
        this._writeOutput(`\r\n[simulator closed: ${reason}]\r\n`);
        this._writeOutput(BRACKETED_PASTE_DISABLE, 'output.mode');
        this._restoreTerminal();
        this._onExit(reason);
    }

    _drainPending(flushAll) {
        let text = this._pendingControlText;
        const limit = flushAll ? text.length : text.length - trailingControlPrefixLength(text);
        let index = 0;

        while (index < limit) {
            if (text.startsWith(BRACKETED_PASTE_START, index)) {
                this._inBracketedPaste = true;
                this._log('input.bracketedPasteStart', {});
                index += BRACKETED_PASTE_START.length;
                continue;
            }

            if (text.startsWith(BRACKETED_PASTE_END, index)) {
                this._inBracketedPaste = false;
                this._log('input.bracketedPasteEnd', {});
                index += BRACKETED_PASTE_END.length;
                continue;
            }

            if (!this._inBracketedPaste && text[index] === '\x1b') {
                const sequenceLength = consumeEscapeSequence(text, index, limit);
                if (sequenceLength === 0) {
                    break;
                }

                this._log('input.control', {
                    sequence: escapeForLog(text.slice(index, index + sequenceLength)),
                });
                index += sequenceLength;
                continue;
            }

            const ch = text[index];
            if (!this._inBracketedPaste && (ch === '\r' || ch === '\n')) {
                if (ch === '\r' && index + 1 < limit && text[index + 1] === '\n') {
                    index += 1;
                }
                this._submitCurrentInput();
                index += 1;
                continue;
            }

            if (!this._inBracketedPaste && (ch === BACKSPACE || ch === ALT_BACKSPACE)) {
                if (this._currentInput.length > 0) {
                    this._currentInput = this._currentInput.slice(0, -1);
                    this._writeOutput('\b \b');
                    this._log('input.backspace', {
                        remainingLength: this._currentInput.length,
                    });
                }
                index += 1;
                continue;
            }

            const nextOrigin = this._inBracketedPaste ? 'paste' : 'typed';
            this._currentOrigin = mergeOrigin(this._currentOrigin, nextOrigin);
            this._currentInput += ch;
            this._writeOutput(ch);
            index += 1;
        }

        this._pendingControlText = text.slice(index);
    }

    _submitCurrentInput() {
        const submittedText = this._currentInput;
        const submitOrigin = this._currentOrigin || 'typed';

        this._writeOutput('\r\n');
        this._log('input.submit', {
            origin: submitOrigin,
            text: submittedText,
            textEscaped: escapeForLog(submittedText),
            length: submittedText.length,
        });

        this._currentInput = '';
        this._currentOrigin = '';

        const trimmed = submittedText.trim();
        if (trimmed.length === 0) {
            this._renderPrompt();
            return;
        }

        if (trimmed === 'exit' || trimmed === 'quit' || trimmed === '/exit') {
            this.shutdown('command-exit');
            return;
        }

        this._writeOutput('Working...\r\n');
        this._schedule(() => {
            if (this._closed) {
                return;
            }

            this._writeOutput(`mode: ${submitOrigin}\r\n`);
            this._writeOutput(`text: ${submittedText}\r\n`);
            this._writeOutput('status: complete\r\n');
            this._log('output.result', {
                origin: submitOrigin,
                text: submittedText,
            });
            this._renderPrompt();
        }, 120);
    }

    _renderPrompt() {
        this._writeOutput(PROMPT, 'output.prompt');
    }

    _writeOutput(text, event = 'output.write') {
        this._write(text);
        this._log(event, {
            text,
            textEscaped: escapeForLog(text),
            length: text.length,
        });
    }
}

function runCli() {
    const logger = createJsonlLogger();

    if (!process.stdin.isTTY || !process.stdout.isTTY) {
        process.stderr.write('This simulator must run in an interactive terminal.\n');
        process.exitCode = 1;
        return;
    }

    const restoreTerminal = () => {
        if (process.stdin.isTTY) {
            process.stdin.setRawMode(false);
        }
        process.stdin.pause();
    };

    process.stdin.setRawMode(true);
    process.stdin.resume();

    const simulator = new CopilotTerminalSimulator({
        log: (event, payload) => logger.append(event, payload),
        restoreTerminal,
        onExit: () => {
            process.stdin.removeAllListeners('data');
        },
    });

    logger.append('session.bootstrap', {
        logPath: logger.logPath,
    });
    simulator.start();

    process.stdin.on('data', (buffer) => {
        simulator.handleBuffer(buffer);
    });

    process.on('SIGINT', () => {
        simulator.shutdown('sigint');
    });
    process.on('SIGTERM', () => {
        simulator.shutdown('sigterm');
    });
}

module.exports = {
    CopilotTerminalSimulator,
    createJsonlLogger,
    defaultLogPath,
    formatLocalTimestamp,
    resolveLogsDirectory,
};

if (require.main === module) {
    runCli();
}
