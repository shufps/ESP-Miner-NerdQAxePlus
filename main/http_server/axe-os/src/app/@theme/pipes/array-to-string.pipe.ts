import { Pipe, PipeTransform } from "@angular/core";


@Pipe({
  name: "arrayToString"
})
export class ArrayToStringPipe implements PipeTransform {
  transform(input: string[] | IData[], sep = ","): string | undefined {

    if(input.length == 0){
      return;
    }
    
    if(typeof input[0] === "string" ){
      return input.join(sep);
    }
    return input.map((list) => list.nome).join(sep);
  }
}

class IData{
  _id!: string
  nome!: string
}
