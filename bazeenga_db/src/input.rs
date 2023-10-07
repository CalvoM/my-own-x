use std::io::{self, Write};
pub struct InputBuffer {
    pub buffer: String,
}

impl InputBuffer {
    pub fn new() -> Self {
        Self {
            buffer: String::new(),
        }
    }
    pub fn read_input(&mut self) {
        self.buffer.clear();
        let _ = io::stdin().read_line(&mut self.buffer);
        if self.buffer.len() <= 0 {
            panic!("Error reading input");
        }
    }
    pub fn prompt(&self) {
        print!("bazeenga_db > ");
        io::stdout().flush().expect("Flush failed");
    }
}
