pub mod input;

use input::InputBuffer;

fn main() {
    let mut input_buffer = InputBuffer::new();
    loop {
        input_buffer.prompt();
        input_buffer.read_input();
        match input_buffer.buffer.trim() {
            ".exit" => std::process::exit(0),
            "" => println!(),
            _ => println!("Unrecognized command: {}", input_buffer.buffer),
        }
    }
}
