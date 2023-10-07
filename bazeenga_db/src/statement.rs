#![allow(non_camel_case_types)]
use crate::input;
pub enum StatementType {
    NONE,
    INSERT,
    SELECT,
}
impl Default for StatementType {
    fn default() -> Self {
        StatementType::NONE
    }
}
pub enum PrepareStatementResult {
    SUCCESS,
    UNRECOGNIZED_STATEMENT,
}

#[derive(Default)]
pub struct Statement {
    stype: StatementType,
}
impl Statement {
    pub fn prepare(&mut self, input_buffer: &input::InputBuffer) -> PrepareStatementResult {
        match input_buffer.buffer.trim() {
            "insert" => {
                self.stype = StatementType::INSERT;
                return PrepareStatementResult::SUCCESS;
            }
            "select" => {
                self.stype = StatementType::SELECT;
                return PrepareStatementResult::SUCCESS;
            }
            _ => return PrepareStatementResult::UNRECOGNIZED_STATEMENT,
        }
    }
    pub fn execute(&mut self) {
        match self.stype {
            StatementType::INSERT => {
                println!("This is where  we would do an insert.");
            }
            StatementType::SELECT => {
                println!("This is where  we would do a select.");
            }
            _ => return,
        }
    }
}
