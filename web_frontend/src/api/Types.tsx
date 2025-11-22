export interface Profile {
	id?: number;
	name: string;
	controller1: string;
	controller2?: string;
}

export interface Mapping {
	id?: number;
	name: string
	controller: number;
	button: number;
	action: string;
	isAxis: boolean;
	deadzone?: number;
}