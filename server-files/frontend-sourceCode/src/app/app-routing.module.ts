import { NgModule } from '@angular/core';
import { Routes, RouterModule } from '@angular/router';
import { LoginComponent } from './components/login/login.component';
import { AddDataComponent } from './components/add-data/add-data.component';
import { ViewDataComponent } from './components/view-data/view-data.component';


const routes: Routes = [
  { path: '', redirectTo: 'view', pathMatch: 'full' },
  { path: 'login', component: LoginComponent },
  { path: 'add', component: AddDataComponent },
  { path: 'view', component: ViewDataComponent }
];

@NgModule({
  imports: [RouterModule.forRoot(routes)],
  exports: [RouterModule]
})
export class AppRoutingModule { }
