export type Task = {
    name: string;
    completed?: boolean;
    subTasks?: Task[];
    points?: number;
    checked?: boolean;
    single?: boolean;
    };

export type SubTask = {
    name: string;
    completed: boolean;
    subTasks?: SubTask[];
    points?: number;
    single?: boolean;
};