pub mod input;
pub mod meta_command;
pub mod statement;

use input::InputBuffer;

fn main() {
    let mut input_buffer = InputBuffer::new();
    loop {
        input_buffer.prompt();
        input_buffer.read_input();
        let mut statement = statement::Statement::default();
        match input_buffer.buffer.chars().nth(0).unwrap() {
            '.' => match meta_command::execute(&input_buffer) {
                meta_command::MetaCommandResult::SUCCESS => continue,
                meta_command::MetaCommandResult::UNRECOGNIZED_COMMAND => {
                    println!("Unrecognized command at start of {}", input_buffer.buffer)
                }
            },
            _ => match statement.prepare(&input_buffer) {
                statement::PrepareStatementResult::SUCCESS => statement.execute(),
                statement::PrepareStatementResult::UNRECOGNIZED_STATEMENT => {
                    println!("Unrecognized keyword at start of {}", input_buffer.buffer);
                }
            },
        }
    }
}
