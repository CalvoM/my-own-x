#![allow(non_camel_case_types)]
use crate::input;
pub enum MetaCommandResult {
    SUCCESS,
    UNRECOGNIZED_COMMAND,
}

pub fn execute(input_buffer: &input::InputBuffer) -> MetaCommandResult {
    match input_buffer.buffer.trim() {
        ".exit" => std::process::exit(0),
        "" => return MetaCommandResult::SUCCESS,
        _ => return MetaCommandResult::UNRECOGNIZED_COMMAND,
    }
}
